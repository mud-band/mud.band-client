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
 * Semi-trivial functions to handle HTTP header timestamps according to
 * RFC 2616 section 3.3.
 *
 * In the highly unlikely event of performance trouble, handbuilt versions
 * would likely be faster than relying on the OS time functions.
 *
 * We must parse three different formats:
 *       000000000011111111112222222222
 *       012345678901234567890123456789
 *       ------------------------------
 *	"Sun, 06 Nov 1994 08:49:37 GMT"		RFC822 & RFC1123
 *	"Sunday, 06-Nov-94 08:49:37 GMT"	RFC850
 *	"Sun Nov  6 08:49:37 1994"		ANSI-C asctime()
 *
 * And always output the RFC1123 format.
 *
 */

#ifdef WIN32
#define __func__ __FUNCTION__
#endif

#include <stdio.h>
#include <time.h>

#include "odr.h"
#include "vassert.h"
#include "vtim.h"

double
VTIM_now(void)
{
	struct odr_timeval tv;

	assert(ODR_gettimeofday(&tv, NULL) == 0);
	return (tv.tv_sec + 1e-6 * tv.tv_usec);
}

double
VTIM_real(void)
{
	struct odr_timespec ts;

	assert(ODR_clock_gettime(ODR_CLOCK_REALTIME, &ts) == 0);
	return (ts.tv_sec + 1e-9 * ts.tv_nsec);
}

double
VTIM_mono(void)
{
	struct odr_timespec ts;

	assert(ODR_clock_gettime(ODR_CLOCK_MONOTONIC, &ts) == 0);
	return (ts.tv_sec + 1e-9 * ts.tv_nsec);
}

struct odr_timeval
VTIM_timeval(double t)
{
	struct odr_timeval tv;

	tv.tv_sec = (time_t)ODR_trunc(t);
	tv.tv_usec = (int)(1e6 * (t - tv.tv_sec));
	return (tv);
}

#if defined(__linux__) || defined(__APPLE__)
#include <math.h>
#include <unistd.h>

void
VTIM_format(char *p, const char *fmt, double t)
{
	struct tm tm;
	time_t tt;
	int r;

	tt = (time_t) t;
	(void)gmtime_r(&tt, &tm);
	r = (int)strftime(p, VTIM_FORMAT_SIZE, fmt, &tm);
	if (r == 0) {
		/*
		 * XXX: If it failed to do the time formatting, it returns
		 * the default value.
		 */
		snprintf(p, VTIM_FORMAT_SIZE, "Thu, 01 Jan 1970 00:00:00 GMT");
	}
}

void
VTIM_sleep(double t)
{
#ifdef HAVE_NANOSLEEP
	struct timespec ts;

	ts = TIM_timespec(t);

	(void)nanosleep(&ts, NULL);
#else
	if (t >= 1.) {
		(void)sleep(floor(t));
		t -= floor(t);
	}
	/* XXX: usleep() is not mandated to be thread safe */
	t *= 1e6;
	if (t > 0)
		(void)usleep(floor(t));
#endif
}
#elif defined(WIN32)
void
VTIM_format(char *p, const char *fmt, double t)
{
	struct tm tm;
	time_t tt;
	int r;

	tt = (time_t) t;
	(void)gmtime_s(&tm, &tt);
	r = strftime(p, VTIM_FORMAT_SIZE, fmt, &tm);
	if (r == 0) {
		/*
		 * XXX: If it failed to do the time formatting, it returns
		 * the default value.
		 */
		snprintf(p, VTIM_FORMAT_SIZE, "Thu, 01 Jan 1970 00:00:00 GMT");
	}
}
#endif
