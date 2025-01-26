/*-
 * Copyright (c) 2006 Verdens Gang AS
 * Copyright (c) 2006-2011 Varnish Software AS
 * All rights reserved.
 *
 * Author: Poul-Henning Kamp <phk@phk.freebsd.dk>
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
 */

#if defined(WIN32)
#define __func__ __FUNCTION__
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#else
#error "Unsupported OS"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "miniobj.h"
#include "odr.h"
#include "odr_pthread.h"
#include "vassert.h"
#include "vsb.h"
#include "vtc_log.h"
#include "vsock.h"

/*--------------------------------------------------------------------*/

int
VSOCK_port(const struct sockaddr_storage *addr)
{

	if (addr->ss_family == AF_INET) {
		const struct sockaddr_in *ain = (const void *)addr;
		return (ntohs((ain->sin_port)));
	}
	if (addr->ss_family == AF_INET6) {
		const struct sockaddr_in6 *ain = (const void *)addr;
		return (ntohs((ain->sin6_port)));
	}
	return (-1);
}

/*--------------------------------------------------------------------*/

void
VSOCK_name(const struct sockaddr_storage *addr, unsigned l,
    char *abuf, unsigned alen, char *pbuf, unsigned plen)
{
	int i;

	i = getnameinfo((const void *)addr, l, abuf, alen, pbuf, plen,
	   NI_NUMERICHOST | NI_NUMERICSERV);
	if (i) {
		/*
		 * XXX this printf is shitty, but we may not have space
		 * for the gai_strerror in the bufffer :-(
		 */
		printf("getnameinfo = %d %s\n", i, gai_strerror(i));
#if defined(WIN32)
		(void)_snprintf(abuf, alen, "Conversion");
		(void)_snprintf(pbuf, plen, "Failed");
#elif defined(__linux__) || defined(__APPLE__)
		(void)snprintf(abuf, alen, "Conversion");
		(void)snprintf(pbuf, plen, "Failed");
#else
#error "Unsupported OS"
#endif
		return;
	}
	/* XXX dirty hack for v4-to-v6 mapped addresses */
	if (strncmp(abuf, "::ffff:", 7) == 0) {
		for (i = 0; abuf[i + 7]; ++i)
			abuf[i] = abuf[i + 7];
		abuf[i] = '\0';
	}
}

/*--------------------------------------------------------------------*/

void
VSOCK_myname(int sock, char *abuf, unsigned alen, char *pbuf, unsigned plen)
{
	struct sockaddr_storage addr_s;
	socklen_t l;

	l = sizeof addr_s;
	AZ(getsockname(sock, (void *)&addr_s, &l));
	VSOCK_name(&addr_s, l, abuf, alen, pbuf, plen);
}
/*--------------------------------------------------------------------*/

void
VSOCK_hisname(int sock, char *abuf, unsigned alen, char *pbuf, unsigned plen)
{
	struct sockaddr_storage addr_s;
	socklen_t l;

	l = sizeof addr_s;
	if (!getpeername(sock, (void*)&addr_s, &l))
		VSOCK_name(&addr_s, l, abuf, alen, pbuf, plen);
	else {
#if defined(WIN32)
		(void)_snprintf(abuf, alen, "<none>");
		(void)_snprintf(pbuf, plen, "<none>");
#elif defined(__linux__) || defined(__APPLE__)
		(void)snprintf(abuf, alen, "<none>");
		(void)snprintf(pbuf, plen, "<none>");
#else
#error "Unsupported OS"
#endif
	}
}

int
VSOCK_blocking(int sock)
{
#if defined(WIN32)
	u_long mode = 0;
	int r;

	r = ioctlsocket(sock, FIONBIO, &mode);
	assert(r == NO_ERROR);
	return (0);
#elif defined(__linux__) || defined(__APPLE__)
	int i, j;

	i = 0;
	j = ioctl(sock, FIONBIO, &i);
	VSOCK_Assert(j);
	return (j);
#else
#error "Unsupported OS"
#endif
}

/*
 * Set the socket I/O mode: In this case FIONBIO enables or disables the
 * blocking mode for the socket based on the numerical value of mode.
 *
 *     If mode = 0, blocking is enabled; 
 *     If mode != 0, non-blocking mode is enabled.
 */
int
VSOCK_nonblocking(int sock)
{
#if defined(WIN32)
	u_long mode = 1;
	int r;

	r = ioctlsocket(sock, FIONBIO, &mode);
	if (r != NO_ERROR)
		return (-1);
#elif defined(__linux__) || defined(__APPLE__)
	int i, j;

	i = 1;
	j = ioctl(sock, FIONBIO, &i);
	VSOCK_Assert(j);
#else
#error "Unsupported OS"
#endif
	return (0);
}

int
VSOCK_reuseaddr(int sock)
{
	int v = 1;

	return (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&v,
	    (int)sizeof(v)));
}

void
VSOCK_setTimeout(int s, int sec)
{
	int ret;
#if defined(WIN32)
	DWORD timeout;

	assert(sec > 0);

	timeout = sec * 1000;
	ret = setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeout,
	    sizeof timeout);
	assert(ret == 0);
	timeout = sec * 1000;
	ret = setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (void *)&timeout,
	    sizeof timeout);
#elif defined(__linux__) || defined(__APPLE__)
	struct timeval tv;

	assert(sec > 0);

	tv.tv_sec = sec;
	tv.tv_usec = 0;
	ret = setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (void *)&tv, sizeof tv);
	assert(ret == 0);
	tv.tv_sec = sec;
	tv.tv_usec = 0;
	ret = setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (void *)&tv, sizeof tv);
	assert(ret == 0);
#else
#error "Unsupported OS"
#endif
}

void
VSOCK_close(int *s)
{
	int i;

#if defined(WIN32)
	i = closesocket(*s);
#else
	i = close(*s);
#endif
	assert(i == 0);
	*s = -1;
}
