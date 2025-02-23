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

#ifndef _LIBMUD_VHTTPS_H
#define	_LIBMUD_VHTTPS_H

#include "vsb.h"
#include "vss.h"
#include "vssl.h"

struct vsb;
struct vtclog;

struct vhttps_internal {
	unsigned		magic;
#define VHTTPS_MAGIC		0x2f02169c
	int			type;
#define	VHTTPS_T_SOCKET		1
#define	VHTTPS_T_BUFFER		2
	int			fd;
	char			*buf;
	size_t			buflen;
	size_t			bufoff;
	struct vtclog		*vl;
	struct vsb		*vsb;
	struct vssl		*ssl;

	int			nrxbuf;
	char			*rxbuf;
	int			prxbuf;
	char			*body;
	unsigned		bodylen;
	char			bodylenstr[20];
	char			chunklen[20];
	char			*gzipbody;
	unsigned		gzipbodylen;
	int			timeout;

#define VHTTPS_MAX_HDR		50
	char			*req[VHTTPS_MAX_HDR];
	char			*resp[VHTTPS_MAX_HDR];
};

struct vhttps_req {
	struct vtclog		*vl;
	unsigned		f_need_resp_mudband_etag : 1,
				f_need_resp_status : 1,
				f_unused : 30;
	const char		*server;
	const char		*domain;
	const char		*url;
	const char		*hdrs;
	const char		*body;
	int			bodylen;
	int			resp_status;
	char			resp_mudband_etag[64];
};

void	VHTTPS_init(void);
int	VHTTPS_get(struct vhttps_req *req, char *respbuf, size_t *resplen);
int	VHTTPS_post(struct vhttps_req *req, char *respbuf, size_t *resplen);

#endif
