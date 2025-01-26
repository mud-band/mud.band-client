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
 *
 */

#ifndef _LIBMUD_VSOCK_H
#define	_LIBMUD_VSOCK_H

#if defined(WIN32)
#elif defined(__linux__) || defined(__APPLE__)
#include <netinet/in.h>
#else
#error "Unsupported OS"
#endif

#include <errno.h>

/* NI_MAXHOST and NI_MAXSERV are ridiculously long for numeric format */
#define VSOCK_ADDRBUFSIZE		64
#define VSOCK_PORTBUFSIZE		16

#ifdef WIN32
#define VSOCK_Assert(a) a
#else
static inline int
VSOCK_Check(int a)
{
	if (a == 0)
		return (1);
	if (errno == ECONNRESET || errno == ENOTCONN)
		return (1);
#if (defined (__SVR4) && defined (__sun)) || defined (__NetBSD__)
	/*
	 * Solaris returns EINVAL if the other end unexepectedly reset the
	 * connection.
	 * This is a bug in Solaris and documented behaviour on NetBSD.
	 */
	if (errno == EINVAL || errno == ETIMEDOUT)
		return (1);
#endif
	return (0);
}
#define VSOCK_Assert(a) assert(VSOCK_Check(a))
#endif

int	VSOCK_port(const struct sockaddr_storage *addr);
void	VSOCK_myname(int sock, char *abuf, unsigned alen, char *pbuf,
	    unsigned plen);
int	VSOCK_blocking(int sock);
int	VSOCK_nonblocking(int sock);
int	VSOCK_reuseaddr(int sock);
void	VSOCK_name(const struct sockaddr_storage *addr, unsigned l,
	    char *abuf, unsigned alen, char *pbuf, unsigned plen);
void	VSOCK_hisname(int sock, char *abuf, unsigned alen, char *pbuf,
	    unsigned plen);
void	VSOCK_setTimeout(int s, int sec);
void	VSOCK_close(int *s);

#endif
