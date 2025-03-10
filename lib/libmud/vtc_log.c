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

#ifdef WIN32
#define __func__ __FUNCTION__
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "miniobj.h"
#include "odr.h"
#include "odr_pthread.h"
#include "vassert.h"
#include "vsb.h"
#include "vtc_log.h"
#include "vtim.h"

static const char * const lead[] = {
	"[ERROR]",
	"[WARN]",
	"[INFO]",
	"[DEBUG]",
	"[TRACE]"
};
static odr_pthread_mutex_t vtc_mtx;

int vtc_error;	/* Error encountered */
unsigned vtc_verbose = 2;

#define NLEAD (sizeof(lead)/sizeof(lead[0]))

/*--------------------------------------------------------------------*/

const char *
vtc_lead(int lvl)
{

	return (lead[lvl]);
}

struct vtclog *
vtc_logopen(const char *id,
    int (*printf_cb)(const char *id, int lvl, double t_elapsed, const char *msg))
{
	struct vtclog *vl;

	ALLOC_OBJ(vl, VTCLOG_MAGIC);
	AN(vl);
	vl->id = id;
	vl->vsb = vsb_newauto();
	vl->printf_cb = printf_cb;
	vl->fp = stdout;
	return (vl);
}

void
vtc_logclose(struct vtclog *vl)
{

	CHECK_OBJ_NOTNULL(vl, VTCLOG_MAGIC);
	vsb_delete(vl->vsb);
	FREE_OBJ(vl);
}

static double vtc_t_first = 0.0;

static void
vtc_log_emit(struct vtclog *vl, unsigned lvl)
{
	double now;
	int ret;
	char nowstr[ODR_TIME_FORMAT_SIZE];

	now = VTIM_now();
	if (vtc_t_first == 0.0)
		vtc_t_first = now;
	if (vl->printf_cb != NULL) {
		ret = vl->printf_cb(vl->id, lvl, now - vtc_t_first, vsb_data(vl->vsb));
		if (ret >= 0)
			return;
	}
	ODR_TimeFormat(nowstr, "%a, %d %b %Y %T GMT", ODR_real());
	fprintf(vl->fp, "%s [%f] %-4s %s ", nowstr, now - vtc_t_first,
	    vl->id, lead[lvl]);
	(void)fputs(vsb_data(vl->vsb), vl->fp);
}

void
vtc_log(struct vtclog *vl, unsigned lvl, const char *fmt, ...)
{
	va_list ap;

	AN(vl);
	CHECK_OBJ_NOTNULL(vl, VTCLOG_MAGIC);
	assert(lvl < NLEAD);
	if (lvl > vtc_verbose)
		return;
	AZ(ODR_pthread_mutex_lock(&vtc_mtx));
	vsb_clear(vl->vsb);
	va_start(ap, fmt);
	(void)vsb_vprintf(vl->vsb, fmt, ap);
	va_end(ap);
	vsb_putc(vl->vsb, '\n');
	vsb_finish(vl->vsb);
	AZ(vsb_overflowed(vl->vsb));

	vtc_log_emit(vl, lvl);

	vsb_clear(vl->vsb);
	AZ(ODR_pthread_mutex_unlock(&vtc_mtx));
	if (lvl == 0)
		vtc_error = 1;
}

void
vtc_dumpln(struct vtclog *vl, unsigned lvl, const char *str, int len)
{
	int l;
	const char *p = str, *q = str;

	for (l = 0; l < len; l++, q++) {
		if (*q == '\n') {
			vtc_log(vl, lvl, "  %.*s", (int)(q - p), p);
			p = q + 1;
			q = q + 1;
		}
	}
}

void
vtc_dump(struct vtclog *vl, unsigned lvl, const char *pfx, const char *str,
    int len)
{
	int l;
	int nl = 1, olen;

	CHECK_OBJ_NOTNULL(vl, VTCLOG_MAGIC);
	assert(lvl < NLEAD);
	if (lvl > vtc_verbose)
		return;
	AZ(ODR_pthread_mutex_lock(&vtc_mtx));
	vsb_clear(vl->vsb);
	if (pfx == NULL)
		pfx = "";
	if (str == NULL)
		vsb_printf(vl->vsb, "%s(null)\n", pfx);
	else {
		olen = len;
		if (len < 0)
			len = (int)strlen(str);
		for (l = 0; l < len; l++, str++) {
			if (l > 4096 && olen != -2) {
				vsb_printf(vl->vsb, "...");
				break;
			}
			if (nl) {
				vsb_printf(vl->vsb, "%s| ", pfx);
				nl = 0;
			}
			if (*str == '\r')
				vsb_printf(vl->vsb, "\\r");
			else if (*str == '\t')
				vsb_printf(vl->vsb, "\\t");
			else if (*str == '\n') {
				vsb_printf(vl->vsb, "\\n\n");
				nl = 1;
			} else if (*str < 0x20 || *str > 0x7e)
				vsb_printf(vl->vsb, "\\x%02x", *str & 0xff);
			else
				vsb_printf(vl->vsb, "%c", *str);
		}
	}
	if (!nl)
		vsb_printf(vl->vsb, "\n");
	vsb_finish(vl->vsb);
	AZ(vsb_overflowed(vl->vsb));

	vtc_log_emit(vl, lvl);

	vsb_clear(vl->vsb);
	AZ(ODR_pthread_mutex_unlock(&vtc_mtx));
}

void
vtc_loginit(void)
{

	AZ(ODR_pthread_mutex_init(&vtc_mtx, NULL));
}
