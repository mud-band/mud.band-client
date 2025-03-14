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

#ifndef LIBMUD_VTC_H
#define	LIBMUD_VTC_H

/*--------------------------------------------------------------------*/

#define	VTCLOG_LEVEL_ERROR	0
#define	VTCLOG_LEVEL_WARNING	1
#define	VTCLOG_LEVEL_INFO	2
#define	VTCLOG_LEVEL_DEBUG	3
#define	VTCLOG_LEVEL_SPAM	4

struct vtclog {
	unsigned		magic;
#define VTCLOG_MAGIC		0x82731202
	const char		*id;
	struct vsb		*vsb;
	int			(*printf_cb)(const char *id, int lvl, double t_elapsed,
				    const char *msg);
	FILE			*fp;
};

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned vtc_verbose;
extern int vtc_error;

struct vtclog *
	vtc_logopen(const char *id,
	    int (*printf)(const char *id, int lvl, double t_elapsed, const char *msg));
void	vtc_log(struct vtclog *vl, unsigned lvl, const char *fmt, ...);
void	vtc_dumpln(struct vtclog *vl, unsigned lvl, const char *str, int len);
void	vtc_dump(struct vtclog *vl, unsigned lvl, const char *pfx,
	    const char *str, int len);
void	vtc_logclose(struct vtclog *vl);
const char *
	vtc_lead(int lvl);
void	vtc_loginit(void);

#ifdef __cplusplus
};
#endif

#endif
