/*-
 * Copyright (c) 2006 Verdens Gang AS
 * Copyright (c) 2006-2010 Varnish Software AS
 * All rights reserved.
 *
 * Author: Dag-Erling Smørgrav <des@des.no>
 * Author: Cecilie Fritzvold <cecilihf@linpro.no>
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
#pragma warning(disable: 4267)
#pragma warning(disable: 4244)
#else
#define	closesocket	close		/* XXX Trick */
#include <sys/time.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "odr.h"
#include "vassert.h"
#include "vss.h"
#include "vsock.h"

/*lint -esym(754, _storage) not ref */

/*
 * Take a string provided by the user and break it up into address and
 * port parts.  Examples of acceptable input include:
 *
 * "localhost" - "localhost:80"
 * "127.0.0.1" - "127.0.0.1:80"
 * "0.0.0.0" - "0.0.0.0:80"
 * "[::1]" - "[::1]:80"
 * "[::]" - "[::]:80"
 *
 * See also RFC5952
 */

int
VSS_parse(const char *str, char **addr, char **port, enum vss_error *errorp)
{
	const char *p;

	*addr = *port = NULL;

	if (str[0] == '[') {
		/* IPv6 address of the form [::1]:80 */
		if ((p = strchr(str, ']')) == NULL ||
		    p == str + 1 ||
		    (p[1] != '\0' && p[1] != ':')) {
			if (errorp != NULL)
				*errorp = VSS_ERROR_INVALID_FORMAT;
			return (-1);
		}
		*addr = ODR_strdup(str + 1);
		AN(*addr);
		(*addr)[p - (str + 1)] = '\0';
		if (p[1] == ':') {
			*port = ODR_strdup(p + 2);
			AN(*port);
		}
	} else {
		/* IPv4 address of the form 127.0.0.1:80, or non-numeric */
		p = strchr(str, ' ');
		if (p == NULL)
			p = strchr(str, ':');
		if (p == NULL) {
			*addr = ODR_strdup(str);
			AN(*addr);
		} else {
			if (p > str) {
				*addr = ODR_strdup(str);
				AN(*addr);
				(*addr)[p - str] = '\0';
			}
			*port = ODR_strdup(p + 1);
			AN(*port);
		}
	}
	return (0);
}

/*
 * For a given host and port, return a list of struct vss_addr, which
 * contains all the information necessary to open and bind a socket.  One
 * vss_addr is returned for each distinct address returned by
 * getaddrinfo().
 *
 * The value pointed to by the tap parameter receives a pointer to an
 * array of pointers to struct vss_addr.  The caller is responsible for
 * freeing each individual struct vss_addr as well as the array.
 *
 * The return value is the number of addresses resoved, or zero.
 *
 * If the addr argument contains a port specification, that takes
 * precedence over the port argument.
 *
 * XXX: We need a function to free the allocated addresses.
 */
int
VSS_resolve(const char *addr, const char *port, struct vss_addr ***vap,
    enum vss_error *errorp)
{
	struct addrinfo hints, *res0, *res;
	struct vss_addr **va;
	int i, ret;
	char *adp, *hop;

	*vap = NULL;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;	/* XXX: IPv4 only for now */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	ret = VSS_parse(addr, &hop, &adp, errorp);
	if (ret)
		return (0);

	if (adp == NULL)
		ret = getaddrinfo(addr, port, &hints, &res0);
	else
		ret = getaddrinfo(hop, adp, &hints, &res0);

	free(hop);
	free(adp);

	if (ret != 0) {
		if (errorp != NULL)
			*errorp = VSS_ERROR_GETADDRINFO;
		return (0);
	}

	AN(res0);
	for (res = res0, i = 0; res != NULL; res = res->ai_next, ++i)
		/* nothing */ ;
	if (i == 0) {
		if (errorp != NULL)
			*errorp = VSS_ERROR_EMPTYADDRINFO;
		freeaddrinfo(res0);
		return (0);
	}
	va = calloc(i, sizeof *va);
	AN(va);
	*vap = va;
	for (res = res0, i = 0; res != NULL; res = res->ai_next, ++i) {
		va[i] = calloc(1, sizeof(**va));
		AN(va[i]);
		va[i]->va_family = res->ai_family;
		va[i]->va_socktype = res->ai_socktype;
		va[i]->va_protocol = res->ai_protocol;
		va[i]->va_addrlen = res->ai_addrlen;
		assert(va[i]->va_addrlen <= sizeof va[i]->va_addr);
		memcpy(&va[i]->va_addr, res->ai_addr, va[i]->va_addrlen);
	}
	freeaddrinfo(res0);
	return (i);
}

int
VSS_resolve_first_ipv4(const char *addr, const char *port, uint32_t *addrp,
    enum vss_error *errorp)
{
	struct vss_addr **vaddr;
	int found = 0, n, nvaddr;

	nvaddr = VSS_resolve(addr, port, &vaddr, errorp);
	for (n = 0; n < nvaddr; n++) {
		struct vss_addr *va = vaddr[n];

		if (va->va_family == AF_INET) {
			struct sockaddr_in *sin;

			sin = (struct sockaddr_in *)&va->va_addr;
			*addrp = sin->sin_addr.s_addr;
			found = 1;
			break;
		}
	}
	for (n = 0; n < nvaddr; n++)
		free(vaddr[n]);
	free(vaddr);
	return (found);
}

/*
 * Given a struct vss_addr, open a socket of the appropriate type, and bind
 * it to the requested address.
 *
 * If the address is an IPv6 address, the IPV6_V6ONLY option is set to
 * avoid conflicts between INADDR_ANY and IN6ADDR_ANY.
 */

int
VSS_bind(const struct vss_addr *va)
{
	int sd, val;

	sd = socket(va->va_family, va->va_socktype, va->va_protocol);
	if (sd < 0) {
		perror("socket()");
		return (-1);
	}
	val = 1;
	if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (const char *)&val,
	    sizeof val) != 0) {
		perror("setsockopt(SO_REUSEADDR, 1)");
		(void)closesocket(sd);
		return (-1);
	}
	if (bind(sd, (const void*)&va->va_addr, va->va_addrlen) != 0) {
		perror("bind()");
		(void)closesocket(sd);
		return (-1);
	}
	return (sd);
}

/*
 * Given a struct vss_addr, open a socket of the appropriate type, bind it
 * to the requested address, and start listening.
 *
 * If the address is an IPv6 address, the IPV6_V6ONLY option is set to
 * avoid conflicts between INADDR_ANY and IN6ADDR_ANY.
 */
int
VSS_listen(const struct vss_addr *va, int depth)
{
	int sd;

	sd = VSS_bind(va);
	if (sd >= 0)  {
		if (listen(sd, depth) != 0) {
			perror("listen()");
			(void)closesocket(sd);
			return (-1);
		}
	}
	return (sd);
}

/*
 * Connect to the socket specified by the address info in va.
 * Return the socket.
 */
int
VSS_connect(const struct vss_addr *va, int nonblock, enum vss_error *errorp,
    int *errornum)
{
	int sd, i;

	sd = socket(va->va_family, va->va_socktype, va->va_protocol);
	if (sd < 0) {
		if (errno != EPROTONOSUPPORT)
			perror("socket()");
		if (errorp != NULL)
			*errorp = VSS_ERROR_SOCKET;
		if (errornum != NULL)
			*errornum = errno;
		return (-1);
	}
	if (nonblock) {
		i = VSOCK_nonblocking(sd);
		if (i == -1) {
			if (errorp != NULL)
				*errorp = VSS_ERROR_IOCTL;
			if (errornum != NULL) {
				*errornum = ODR_n_errno();
			}
			return (-1);
		}
	}
	i = connect(sd, (const void *)&va->va_addr, va->va_addrlen);
#if defined(WIN32)
	if (i == 0 || (nonblock && WSAGetLastError() == WSAEWOULDBLOCK)) {
		VSOCK_blocking(sd);
		return (sd);
	}
#else
	if (i == 0 || (nonblock && errno == EINPROGRESS)) {
		VSOCK_blocking(sd);
		return (sd);
	}
#endif
	if (errorp != NULL)
		*errorp = VSS_ERROR_CONNECT;
	if (errornum != NULL)
		*errornum = errno;
	(void)closesocket(sd);
	return (-1);
}

/*
 * And the totally brutal version: Give me connection to this address
 */

int
VSS_open(const char *str, double tmo, enum vss_error *errorp, int *errornum)
{
	int retval = -1;
	int nvaddr, n, i;
	struct timeval tv;
	struct vss_addr **vaddr;
	fd_set set;

	if (errorp != NULL)
		*errorp = VSS_ERROR_OK;
	if (errornum != NULL)
		*errornum = 0;

	nvaddr = VSS_resolve(str, NULL, &vaddr, errorp);
	for (n = 0; n < nvaddr; n++) {
		retval = VSS_connect(vaddr[n], tmo != 0.0, errorp, errornum);
		if (retval >= 0 && tmo != 0.0) {
			FD_ZERO(&set);
			FD_SET(retval, &set);
			tv.tv_sec = (int)tmo;
			tv.tv_usec = 0;
			i = select(retval + 1, NULL, &set, NULL, &tv);
			if (i <= 0) {
				(void)closesocket(retval);
				if (errorp != NULL)
					*errorp = VSS_ERROR_SELECT_TIMEOUT;
				retval = -1;
			}
		}
		if (retval >= 0)
			break;
	}
	for (n = 0; n < nvaddr; n++)
		free(vaddr[n]);
	free(vaddr);
	if (retval > 0 && tmo > 0.0)
		VSOCK_setTimeout(retval, (int)tmo);
	return (retval);
}

const char *
VSS_errorstr(enum vss_error error)
{

	switch (error) {
	case VSS_ERROR_OK:
		return "NO_ERROR";
	case VSS_ERROR_INVALID_FORMAT:
		return "INVALID_FORMAT";
	case VSS_ERROR_SELECT_TIMEOUT:
		return "SELECT_TIMEOUT";
	case VSS_ERROR_SOCKET:
		return "SOCKET_ERROR";
	case VSS_ERROR_CONNECT:
		return "CONNECT_ERROR";
	case VSS_ERROR_GETADDRINFO:
		return "GETADDRINFO_ERROR";
	case VSS_ERROR_EMPTYADDRINFO:
		return "EMPTYADDRINFO";
	case VSS_ERROR_IOCTL:
		return "IOCTL_ERROR";
	default:
		break;
	}
	return "UNEXPECTED";
}

const char *
VSS_errornumstr(enum vss_error error, int errornum)
{

	switch (error) {
	case VSS_ERROR_SOCKET:
	case VSS_ERROR_IOCTL:
	case VSS_ERROR_CONNECT:
		return (strerror(errornum));
	default:
		break;
	}
	return "Unknown";
}
