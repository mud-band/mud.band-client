/*-
 * Copyright (c) 2006 Verdens Gang AS
 * Copyright (c) 2006-2009 Varnish Software AS
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

#ifndef _LIBMUD_VSS_H
#define	_LIBMUD_VSS_H

#if defined(WIN32)
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <unistd.h>
#endif

enum vss_error {
	VSS_ERROR_OK = 0,
	VSS_ERROR_INVALID_FORMAT,
	VSS_ERROR_SELECT_TIMEOUT,
	VSS_ERROR_SOCKET,
	VSS_ERROR_CONNECT,
	VSS_ERROR_GETADDRINFO,
	VSS_ERROR_EMPTYADDRINFO,
	VSS_ERROR_IOCTL,
};

/* lightweight addrinfo */
struct vss_addr {
	int			 va_family;
	int			 va_socktype;
	int			 va_protocol;
	int			 va_addrlen;
	struct sockaddr_storage	 va_addr;
};

int	VSS_parse(const char *str, char **addr, char **port,
	    enum vss_error *errorp);
int	VSS_resolve(const char *addr, const char *port, struct vss_addr ***ta,
	    enum vss_error *errorp);
int	VSS_resolve_first_ipv4(const char *addr, const char *port,
	    uint32_t *addrp, enum vss_error *errorp);
int	VSS_bind(const struct vss_addr *addr);
int	VSS_listen(const struct vss_addr *addr, int depth);
int	VSS_connect(const struct vss_addr *addr, int nonblock,
	    enum vss_error *errorp, int *errornum);
int	VSS_open(const char *str, double tmo, enum vss_error *errorp,
	    int *errornum);
const char *
	VSS_errorstr(enum vss_error error);
const char *
	VSS_errornumstr(enum vss_error error, int errornum);

#endif
