/*-
 * Copyright (c) 2016 Weongyo Jeong <weongyo@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#if defined(WIN32)
#define	close closesocket
#else
#include <sys/param.h>
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "miniobj.h"
#include "odr.h"
#include "odr_pthread.h"
#include "vct.h"
#include "vhttps.h"
#include "vgz.h"
#include "vsb.h"
#include "vsock.h"
#include "vtc_log.h"

#define	AN(foo)		do { assert((foo) != 0); } while (0)
#define	AZ(foo)		do { assert((foo) == 0); } while (0)

static struct vhttps_internal *
		vhttps_allocForSocket(struct vtclog *vl, int fd, void *ssl,
		    int timeout);
#if 0
static struct vhttps_internal *
		vhttps_allocForBuffer(struct vtclog *vl, char *buf,
		    size_t buflen);
#endif
static void	vhttps_free(struct vhttps_internal *hp);
static int	vhttps_rxbody(struct vhttps_internal *hp);


static void
vhttps_splitheader(struct vhttps_internal *hp, int req)
{
	char *p, *q, **hh;
	int n;
	char buf[20];

	CHECK_OBJ_NOTNULL(hp, VHTTPS_MAGIC);
	if (req) {
		memset(hp->req, 0, sizeof hp->req);
		hh = hp->req;
	} else {
		memset(hp->resp, 0, sizeof hp->resp);
		hh = hp->resp;
	}

	n = 0;
	p = hp->rxbuf;

	/* REQ/PROTO */
	while (vct_islws(*p))
		p++;
	hh[n++] = p;
	while (!vct_islws(*p))
		p++;
	assert(!vct_iscrlf(*p));
	*p++ = '\0';

	/* URL/STATUS */
	while (vct_issp(*p))		/* XXX: H space only */
		p++;
	assert(!vct_iscrlf(*p));
	hh[n++] = p;
	while (!vct_islws(*p))
		p++;
	if (vct_iscrlf(*p)) {
		hh[n++] = NULL;
		q = p;
		p += vct_skipcrlf(p);
		*q = '\0';
	} else {
		*p++ = '\0';
		/* PROTO/MSG */
		while (vct_issp(*p))		/* XXX: H space only */
			p++;
		hh[n++] = p;
		while (!vct_iscrlf(*p))
			p++;
		q = p;
		p += vct_skipcrlf(p);
		*q = '\0';
	}
	assert(n == 3);

	while (*p != '\0') {
		assert(n < VHTTPS_MAX_HDR);
		if (vct_iscrlf(*p))
			break;
		hh[n++] = p++;
		while (*p != '\0' && !vct_iscrlf(*p))
			p++;
		q = p;
		p += vct_skipcrlf(p);
		*q = '\0';
	}
	p += vct_skipcrlf(p);
	assert(*p == '\0');

	for (n = 0; n < 3 || hh[n] != NULL; n++) {
		sprintf(buf, "http[%2d] ", n);
		vtc_dump(hp->vl, 4, buf, hh[n], -1);
	}
}

static int
vhttps_rxcharForSocket(struct vhttps_internal *hp, int n, int eof)
{
	fd_set set;
	int i;

	while (n > 0) {
		struct timeval tv;

		tv.tv_sec = hp->timeout;
                tv.tv_usec = 0;
		FD_ZERO(&set);
		FD_SET(hp->fd, &set);
		i = select(hp->fd + 1, &set, NULL, NULL, &tv);
		if (i == 0) {
			vtc_log(hp->vl, 1,
			    "BANDEC_00010: HTTP rx timeout (fd:%d %u secs)",
			    hp->fd, hp->timeout);
			return (i);
		}
		if (i < 0) {
			vtc_log(hp->vl, 1,
			    "BANDEC_00011: HTTP rx failed (fd:%d poll: %s)",
			    hp->fd, strerror(errno));
			return (i);
		}
		assert(i > 0);
		assert(hp->prxbuf + n < hp->nrxbuf);
		if (hp->ssl != NULL)
			i = VSSL_read(hp->ssl, hp->rxbuf + hp->prxbuf, n);
		else
			i = recv(hp->fd, hp->rxbuf + hp->prxbuf, n, 0);
		if (i == 0 && eof)
			return (i);
		if (i == 0) {
			vtc_log(hp->vl, 1,
			    "BANDEC_00012: HTTP rx EOF (r:%d fd:%d read: %s)",
			    i, hp->fd, strerror(errno));
			return (i);
		}
		if (i < 0) {
			vtc_log(hp->vl, 1,
			    "BANDEC_00013: HTTP rx failed (fd:%d read: %s)",
			    hp->fd, strerror(errno));
			return (i);
		}
		hp->prxbuf += i;
		hp->rxbuf[hp->prxbuf] = '\0';
		n -= i;
	}
	return (1);
}

static int
vhttps_rxcharForBuffer(struct vhttps_internal *hp, int n, int eof)
{
	size_t l;

	(void)eof;
	assert(hp->prxbuf + n < hp->nrxbuf);
	assert(n >= 0);

	l = MIN(hp->buflen - hp->bufoff, (size_t)n);
	if (l == 0)
		return (0);
	assert(l > 0);
	ODR_bcopy(hp->buf + hp->bufoff, hp->rxbuf + hp->prxbuf, l);
	hp->bufoff += l;
	hp->prxbuf += (int)l;
	hp->rxbuf[hp->prxbuf] = '\0';
	return (1);
}

static int
vhttps_rxchar(struct vhttps_internal *hp, int n, int eof)
{

	if (hp->type == VHTTPS_T_SOCKET)
		return (vhttps_rxcharForSocket(hp, n, eof));
	return (vhttps_rxcharForBuffer(hp, n, eof));
}

static int
vhttps_rxhdr(struct vhttps_internal *hp)
{
	int i, r;
	char *p;

	CHECK_OBJ_NOTNULL(hp, VHTTPS_MAGIC);
	hp->prxbuf = 0;
	hp->body = NULL;
	while (1) {
		r = vhttps_rxchar(hp, 1, 0);
		if (r <= 0)
			return (-1);
		p = hp->rxbuf + hp->prxbuf - 1;
		for (i = 0; p > hp->rxbuf; p--) {
			if (*p != '\n')
				break;
			if (p - 1 > hp->rxbuf && p[-1] == '\r')
				p--;
			if (++i == 2)
				break;
		}
		if (i == 2)
			break;
	}
	vtc_dump(hp->vl, 4, "rxhdr", hp->rxbuf, -1);
	return (0);
}

static char *
vhttps_find_header(char * const *hh, const char *hdr)
{
	int n, l;
	char *r;

	l = (int)strlen(hdr);

	for (n = 3; hh[n] != NULL; n++) {
		if (ODR_strncasecmp(hdr, hh[n], l) || hh[n][l] != ':')
			continue;
		for (r = hh[n] + l + 1; vct_issp(*r); r++)
			continue;
		return (r);
	}
	return (NULL);
}

static int
vhttps_rxchunk(struct vhttps_internal *hp)
{
	char *q;
	int l, i, r;

	l = hp->prxbuf;
	do {
		r = vhttps_rxchar(hp, 1, 0);
		if (r <= 0)
			return (r);
	} while (hp->rxbuf[hp->prxbuf - 1] != '\n');
	vtc_dump(hp->vl, 4, "len", hp->rxbuf + l, -1);
	i = strtoul(hp->rxbuf + l, &q, 16);
	snprintf(hp->chunklen, sizeof(hp->chunklen), "%d", i);
	if (q == hp->rxbuf + l) {
		vtc_log(hp->vl, 1,
		    "BANDEC_00014: Invalid chunk size (no digits found)");
		return (-1);
	}
	if (*q != '\0' && !vct_islws(*q)) {
		vtc_log(hp->vl, 1,
		    "BANDEC_00908: Invalid character after chunk size ('%c')",
		    *q);
		return (-1);
	}
	hp->prxbuf = l;
	if (i > 0) {
		r = vhttps_rxchar(hp, i, 0);
		if (r <= 0) {
			vtc_log(hp->vl, 1,
			    "BANDEC_00909: Failed to read chunk data"
			    " (expected %d bytes)", i);
			return (-1);
		}
		vtc_dump(hp->vl, 4, "chunk", hp->rxbuf + l, i);
	} else if (i < 0) {
		vtc_log(hp->vl, 1,
		    "BANDEC_00910: Invalid negative chunk size (%d)", i);
		return (-1);
	}
	l = hp->prxbuf;
	r = vhttps_rxchar(hp, 2, 0);
	if (r <= 0) {
		vtc_log(hp->vl, 1,
		    "BANDEC_00911: Failed to read chunk terminator");
		return (-1);
	}
	if (!vct_iscrlf(hp->rxbuf[l])) {
		vtc_log(hp->vl, 1,
		    "BANDEC_00015: Wrong chunk tail[0] = %02x",
		    hp->rxbuf[l] & 0xff);
		return (-1);
	}
	if (!vct_iscrlf(hp->rxbuf[l + 1])) {
		vtc_log(hp->vl, 1,
		    "BANDEC_00016: Wrong chunk tail[1] = %02x",
		    hp->rxbuf[l + 1] & 0xff);
		return (-1);
	}
	hp->prxbuf = l;
	hp->rxbuf[l] = '\0';
	return (i);
}

static void
vhttps_swallow_body(struct vhttps_internal *hp, char * const *hh, int body)
{
	char *p;
	int i, l, ll;

	ll = 0;
	p = vhttps_find_header(hh, "content-length");
	if (p != NULL) {
		l = strtoul(p, NULL, 0);
		(void)vhttps_rxchar(hp, l, 0);
		vtc_dump(hp->vl, 4, "body", hp->body, l);
		hp->bodylen = l;
		sprintf(hp->bodylenstr, "%d", l);
		return;
	}
	p = vhttps_find_header(hh, "transfer-encoding");
	if (p != NULL && !strcmp(p, "chunked")) {
		while ((i = vhttps_rxchunk(hp)) > 0)
			continue;
		if (i < 0) {
			vtc_log(hp->vl, 1,
			    "BANDEC_XXXXX: Error reading chunked body.");
			return;
		}
		vtc_dump(hp->vl, 4, "body", hp->body, ll);
		ll = (int)(hp->rxbuf + hp->prxbuf - hp->body);
		hp->bodylen = ll;
		sprintf(hp->bodylenstr, "%d", ll);
		return;
	}
	if (body) {
		hp->body = hp->rxbuf + hp->prxbuf;
		do  {
			i = vhttps_rxchar(hp, 1, 1);
			ll += i;
		} while (i > 0);
		vtc_dump(hp->vl, 4, "rxeof", hp->body, ll);
	}
	hp->bodylen = ll;
	sprintf(hp->bodylenstr, "%d", ll);
}

static struct vhttps_internal *
vhttps_allocCommon(struct vtclog *vl)
{
	struct vhttps_internal *hp;

	ALLOC_OBJ(hp, VHTTPS_MAGIC);
	hp->fd = -1;
	hp->timeout = 3;
	hp->nrxbuf = 1024 * 1024;
	hp->vsb = vsb_newauto();
	hp->rxbuf = malloc(hp->nrxbuf);		/* XXX */
	AN(hp->rxbuf);
	hp->vl = vl;
	AN(hp->rxbuf);
	AN(hp->vsb);
	return (hp);
}

struct vhttps_internal *
vhttps_allocForSocket(struct vtclog *vl, int fd, void *ssl, int timeout)
{
	struct vhttps_internal *hp;

	hp = vhttps_allocCommon(vl);
	AN(hp);
	hp->type = VHTTPS_T_SOCKET;
	hp->fd = fd;
	hp->ssl = ssl;
	hp->timeout = timeout;
	return (hp);
}

#if 0
struct vhttps_internal *
vhttps_allocForBuffer(struct vtclog *vl, char *buf, size_t buflen)
{
	struct vhttps_internal *hp;

	hp = vhttps_allocCommon(vl);
	AN(hp);
	hp->type = VHTTPS_T_BUFFER;
	hp->buf = buf;
	hp->buflen = buflen;
	hp->bufoff = 0;
	return (hp);
}
#endif

void
vhttps_free(struct vhttps_internal *hp)
{

	if (hp == NULL)
		return;
	free(hp->rxbuf);
	free(hp->gzipbody);
	vsb_delete(hp->vsb);
	free(hp);
}

static int
vhttps_rxbody_gzip(struct vhttps_internal *hp)
{
	z_stream strm;
	Bytef *out = NULL;
	uLong outlen, new_outlen;
	int ret;

	if (hp->bodylen == 0)
		return (0);
	outlen = hp->bodylen * 10;
	while (outlen < (1UL << 30)) {
		memset(&strm, 0, sizeof(strm));
		ret = inflateInit2(&strm, 31); // 31 = 15 (max window bits) + 16 (gzip format)
		if (ret != Z_OK) {
			vtc_log(hp->vl, 1,
			    "BANDEC_00796: inflateInit2 failed: %d", ret);
			return (-1);
		}
		if (out != NULL) {
			free(out);
			new_outlen = outlen * 10;
			if (new_outlen < outlen) {
				vtc_log(hp->vl, 1,
				    "BANDEC_00797: Buffer size overflow");
				inflateEnd(&strm);
				return (-1);
			}
			outlen = new_outlen;
		}
		out = (Bytef *)malloc(outlen);
		if (out == NULL) {
			inflateEnd(&strm);
			vtc_log(hp->vl, 1,
			    "BANDEC_00798: Failed to allocate decompression"
			    " buffer");
			return (-1);
		}
		strm.next_in = (Bytef *)hp->body;
		strm.avail_in = hp->bodylen;
		strm.next_out = out;
		strm.avail_out = outlen;
		ret = inflate(&strm, Z_FINISH);
		if (ret == Z_BUF_ERROR) {
			inflateEnd(&strm);
			continue;
		}
		break;
	}
	if (ret != Z_STREAM_END) {
		free(out);
		inflateEnd(&strm);
		vtc_log(hp->vl, 1, "BANDEC_00799: inflate failed: %d", ret);
		return (-1);
	}
	outlen = strm.total_out;
	hp->gzipbody = (char *)out;
	hp->gzipbodylen = (unsigned)outlen;
	hp->body = hp->gzipbody;
	hp->bodylen = hp->gzipbodylen;
	sprintf(hp->bodylenstr, "%d", hp->bodylen);
	inflateEnd(&strm);
	return (0);
}

int
vhttps_rxbody(struct vhttps_internal *hp)
{
	char *p;

	if (vhttps_rxhdr(hp) != 0) {
		vtc_log(hp->vl, 1, "BANDEC_00017: vhttps_rxhdr error.");
		return (-1);
	}
	vhttps_splitheader(hp, 0);
	hp->body = hp->rxbuf + hp->prxbuf;
	if (!strcmp(hp->resp[1], "200"))
		vhttps_swallow_body(hp, hp->resp, 1);
	else
		vhttps_swallow_body(hp, hp->resp, 0);
	p = vhttps_find_header(hp->resp, "content-encoding");
	if (p != NULL && strstr(p, "gzip") != NULL)
		return (vhttps_rxbody_gzip(hp));
	return (0);
}

int
VHTTPS_get(struct vhttps_req *req, char *respbuf, size_t *resplen)
{
	struct vsb *vsb;
	enum vss_error error;
	struct vssl *ssl;
	size_t l;
	int errornum, fd, r;
	int timeout = 30;

	fd = VSS_open(req->server, 10, &error, &errornum);
	if (fd == -1) {
		vtc_log(req->vl, 1,
		    "BANDEC_00018: Failed to communicate with server %s:"
		    " %d %d", req->server, error, errornum);
		return (-1);
	}
	VSOCK_blocking(fd);
	VSOCK_setTimeout(fd, timeout);
	vsb = vsb_newauto();
	AN(vsb);
	ssl = VSSL_new(req->vl, fd, req->domain);
	AN(ssl);
	r = VSSL_connect(ssl);
	if (r == -1) {
		vtc_log(req->vl, 1, "BANDEC_00019: VSSL_connect(3) failed.");
		goto error;
	}
	vsb_printf(vsb, "GET %s HTTP/1.1\r\n", req->url);
	vsb_printf(vsb, "Connection: close\r\n");
	if (req->hdrs != NULL)
		vsb_printf(vsb, "%s", req->hdrs);
	vsb_printf(vsb, "\r\n");
	vsb_finish(vsb);
	vtc_log(req->vl, 4, "%.*s", (int)vsb_len(vsb), vsb_data(vsb));
	l = VSSL_write(ssl, vsb_data(vsb), vsb_len(vsb));
	if (l != vsb_len(vsb)) {
		vtc_log(req->vl, 1,
		    "BANDEC_00020: VHTTPS_get send(2) failed: %ld %d",
		    l, errno);
		goto error;
	} else {
		struct vhttps_internal *hp;

		hp = vhttps_allocForSocket(req->vl, fd, ssl, timeout);
		AN(hp);
		if (vhttps_rxbody(hp) != 0) {
			vtc_log(req->vl, 1,
			    "BANDEC_00021: vhttps_rxbody() error."
				" (server %s url %s)",
			    req->server, req->url);
			vhttps_free(hp);
			goto error;
		}
		AN(resplen);
		if (hp->bodylen > *resplen) {
			vtc_log(req->vl, 1,
			    "BANDEC_00022: Not enough buffer space");
			vhttps_free(hp);
			goto error;
		}
		*resplen = hp->bodylen;
		memcpy(respbuf, hp->body, hp->bodylen);
		if (req->f_need_resp_status)
			req->resp_status = atoi(hp->resp[1]);
		vhttps_free(hp);
	}
	vsb_delete(vsb);
	VSSL_free(ssl);
	close(fd);
	return (0);
error:
	vsb_delete(vsb);
	VSSL_free(ssl);
	close(fd);
	return (-1);
}

int
VHTTPS_post(struct vhttps_req *req, char *respbuf, size_t *resplen)
{
	struct vsb *vsb;
	enum vss_error error;
	struct vssl *ssl;
	size_t l;
	int errornum, fd, r;
	int timeout = 30;

	fd = VSS_open(req->server, 10, &error, &errornum);
	if (fd == -1) {
		vtc_log(req->vl, 1,
		    "BANDEC_00023: Failed to communicate with server (%s):"
		    " %d %d", req->server, error, errornum);
		return (-1);
	}
	VSOCK_blocking(fd);
	VSOCK_setTimeout(fd, timeout);
	vsb = vsb_newauto();
	AN(vsb);
	ssl = VSSL_new(req->vl, fd, req->domain);
	AN(ssl);
	r = VSSL_connect(ssl);
	if (r == -1) {
		vtc_log(req->vl, 1, "BANDEC_00024: VSSL_connect(3) failed.");
		goto error;
	}
	vsb_printf(vsb, "POST %s HTTP/1.1\r\n", req->url);
	vsb_printf(vsb, "Connection: close\r\n");
	vsb_printf(vsb, "Content-Length: %d\r\n", req->bodylen);
	if (req->hdrs != NULL)
		vsb_printf(vsb, "%s", req->hdrs);
	vsb_printf(vsb, "\r\n");
	vsb_printf(vsb, "%s", req->body);
	vsb_finish(vsb);
	vtc_log(req->vl, 4, "%.*s", (int)vsb_len(vsb), vsb_data(vsb));
	l = VSSL_write(ssl, vsb_data(vsb), vsb_len(vsb));
	if (l != vsb_len(vsb)) {
		vtc_log(req->vl, 1,
		    "BANDEC_00025: VHTTPS_get send(2) failed: %ld %d",
		    l, errno);
		goto error;
	} else {
		struct vhttps_internal *hp;

		hp = vhttps_allocForSocket(req->vl, fd, ssl, timeout);
		AN(hp);
		if (vhttps_rxbody(hp) != 0) {
			vtc_log(req->vl, 1, "BANDEC_00026: vhttps_rxbody error");
			vhttps_free(hp);
			goto error;
		}
		AN(resplen);
		if (hp->bodylen > *resplen) {
			vtc_log(req->vl, 1,
			    "BANDEC_00027: Not enough buffer space. %d/%d",
			    (int)hp->bodylen, (int)*resplen);
			vhttps_free(hp);
			goto error;
		}
		*resplen = hp->bodylen;
		memcpy(respbuf, hp->body, hp->bodylen);
		if (req->f_need_resp_status)
			req->resp_status = atoi(hp->resp[1]);
		if (req->f_need_resp_mudband_etag) {
			char *p;

			p = vhttps_find_header(hp->resp, "mudband-etag");
			if (p != NULL) {
				ODR_snprintf(req->resp_mudband_etag,
				    sizeof(req->resp_mudband_etag), "%s", p);
			}
		}
		vhttps_free(hp);
	}
	vsb_delete(vsb);
	VSSL_free(ssl);
	close(fd);
	return (0);
error:
	vsb_delete(vsb);
	VSSL_free(ssl);
	close(fd);
	return (-1);
}

#define	TRUST_ME(ptr)	((void*)(uintptr_t)(ptr))

void
VHTTPS_init(void)
{

	VSSL_init();
}
