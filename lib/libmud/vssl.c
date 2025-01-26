/*-
 * Copyright (c) 2023 Weongyo Jeong <weongyo@gmail.com>
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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
#include "vsb.h"
#include "vssl.h"
#include "vtc_log.h"

#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

static SSL_CTX	*vssl_ctx;
static odr_pthread_mutex_t *vssl_lock;
static int vssl_inited;

static unsigned long
VSSL_thrid_cb(void)
{
#if defined(WIN32)
	odr_pthread_t tp;

	tp = ODR_pthread_self();
	return ((unsigned long long)(tp.p));
#else
	return ((unsigned long)(ODR_pthread_self()));
#endif
}

static void
VSSL_lock_cb(int mode, int type, const char *file, int line)
{

	int n;
	(void)file;
	(void)line;

	assert((mode & (CRYPTO_READ | CRYPTO_WRITE)) !=
	    (CRYPTO_READ | CRYPTO_WRITE));
	if (mode & CRYPTO_LOCK) {
		/*
		 * If neither CRYPTO_READ nor CRYPTO_WRITE defined
		 * assume exclusive lock.
		 */
		if (mode & CRYPTO_READ)
			n = ODR_pthread_mutex_lock(&vssl_lock[type]);
		else
			n = ODR_pthread_mutex_lock(&vssl_lock[type]);
	} else if (mode & CRYPTO_UNLOCK) {
		n = ODR_pthread_mutex_unlock(&vssl_lock[type]);
	} else
		assert(0 == 1);
	assert(n == 0);
}

void
VSSL_init(void)
{
	int i, num, rc;

	(void)VSSL_lock_cb;
	(void)VSSL_thrid_cb;

	if (vssl_inited)
		return;

	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();

	num = CRYPTO_num_locks();
	vssl_lock = (odr_pthread_mutex_t *)OPENSSL_malloc(num *
	    sizeof(odr_pthread_mutex_t));
	ODR_bzero(vssl_lock, num * sizeof(odr_pthread_mutex_t));
	for (i = 0; i < num; i++) {
		rc = ODR_pthread_mutex_init(&vssl_lock[i], NULL);
		assert(rc == 0);
	}
	/* Set callbacks for static locking. */
	CRYPTO_set_id_callback(VSSL_thrid_cb);
	CRYPTO_set_locking_callback(VSSL_lock_cb);

	vssl_ctx = SSL_CTX_new(SSLv23_client_method());
	AN(vssl_ctx);
	vssl_inited = 1;
}

static void
vssl_print_error(struct vtclog *vl)
{
	unsigned long e;
	int i = 0;
	char buf[1024];

	while ((e = ERR_get_error()) != 0) {
		ERR_error_string_n(e, buf, sizeof(buf));
                vtc_log(vl, 1,
                    "BANDEC_00001: ssl_error %lu %s", e, buf);
                i++;
		if (i > 10)
			break;
        }
}

struct vssl *
VSSL_new(struct vtclog *vl, int fd, const char *domain)
{
	struct vssl *s;
	char *name;

	if (vssl_ctx == NULL) {
		vtc_log(vl, 0, "BANDEC_00002: SSL context is null.");
		return (NULL);
	}
	name = ODR_strdup(domain);
	AN(name);
	s = (struct vssl *)malloc(sizeof(*s));
	AN(s);
	s->vl = vl;
	s->ssl = SSL_new(vssl_ctx);
	if (s->ssl == NULL) {
		vssl_print_error(vl);
		vtc_log(vl, 0, "BANDEC_00003: SSL_new() failed.");
		free(s);
		free(name);
		return (NULL);
	}
	SSL_set_fd(s->ssl, fd);
#if defined(SSL_set_tlsext_host_name)
	{
		int r;

		r = SSL_set_tlsext_host_name(s->ssl, name);
		assert(r == 1);
	}
#endif
	free(name);
	return (s);
}

int
VSSL_connect(struct vssl *s)
{
	int errornum, r;

	r = SSL_connect(s->ssl);
	if (r != 1) {
		errornum = SSL_get_error(s->ssl, r);
		switch (errornum) {
		case SSL_ERROR_SYSCALL:
			vtc_log(s->vl, 1,
			    "BANDEC_00004: SSL_connect(3) failed:"
			    " SSL_ERROR_SYSCALL (error %d)",
#if defined(WIN32)
			    WSAGetLastError()
#else
			    errno
#endif
			    );
			break;
		default:
			vtc_log(s->vl, 1,
			    "BANDEC_00005: SSL_connect(3) failed: %d %d  ",
			    r, errornum);
			ERR_print_errors_fp(stdout);
			break;
		}
		ERR_print_errors_fp(stdout);
		return (-1);
	}
	return (0);
}

int
VSSL_read(struct vssl *s, void *buf, size_t buflen)
{
	int error, r;

	r = SSL_read(s->ssl, buf, buflen);
	if (r <= 0) {
		error = SSL_get_error(s->ssl, r);
		switch (error) {
		case SSL_ERROR_SYSCALL:
			vtc_log(s->vl, 1,
			    "BANDEC_00006: SSL_read() failed: SSL_ERROR_SYSCALL (error %d)",
#if defined(WIN32)
			    WSAGetLastError()
#else
			    errno
#endif
			    );
		default:
			vtc_log(s->vl, 1,
			    "BANDEC_00007: SSL_read() failed. (r %d error %d)",
			    r, error);
			break;
		}
		ERR_print_errors_fp(stdout);
		return (r);
	}
	return (r);
}

int
VSSL_write(struct vssl *s, void *buf, size_t buflen)
{
	int error, r;

	r = SSL_write(s->ssl, buf, buflen);
	if (r <= 0) {
		error = SSL_get_error(s->ssl, r);
		switch (error) {
		case SSL_ERROR_SYSCALL:
			vtc_log(s->vl, 1,
			    "BANDEC_00008: SSL_read() failed:"
			    " SSL_ERROR_SYSCALL (error %d)",
#if defined(WIN32)
			    WSAGetLastError()
#else
			    errno
#endif
			    );
		default:
			vtc_log(s->vl, 1,
			    "BANDEC_00009: SSL_write() failed. (r %d error %d)",
			    r, error);
			break;
		}
		return (r);
	}
	return (r);
}

void
VSSL_free(struct vssl *s)
{

	SSL_free(s->ssl);
	free(s);
}
