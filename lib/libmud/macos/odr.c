/*-
 * Copyright (c) 2011-2014 Weongyo Jeong <weongyo@gmail.com>
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

#include <sys/time.h>
#include <sys/times.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netdb.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "odr.h"
#include "vassert.h"
#include "vtc_log.h"

void
ODR_libinit(void)
{
	struct sigaction sac;
	int r;

	srand((uint32_t)time(NULL));

	memset(&sac, 0, sizeof sac);
	sac.sa_handler = SIG_IGN;
	sac.sa_flags = SA_RESTART;
	r = sigaction(SIGPIPE, &sac, NULL);
	assert(r == 0);
}

double
ODR_real(void)
{
	struct odr_timespec ts;

	assert(ODR_clock_gettime(ODR_CLOCK_REALTIME, &ts) == 0);
	return (ts.tv_sec + 1e-9 * ts.tv_nsec);
}

void
ODR_TimeFormat(char *p, const char *fmt, double t)
{
	struct tm tm;
	time_t tt;

	tt = (time_t) t;
	(void)gmtime_r(&tt, &tm);
	strftime(p, ODR_TIME_FORMAT_SIZE, fmt, &tm);
}

int
ODR_gettimeofday(struct odr_timeval *tp, void *tz)
{
	struct timeval tv;
	int ret;

	assert(tz == NULL);
	ret = gettimeofday(&tv, NULL);
	if (ret == -1)
		return (ret);
	tp->tv_sec = tv.tv_sec;
	tp->tv_usec = tv.tv_usec;
	return (ret);
}

double
ODR_trunc(double x)
{

	return (trunc(x));
}

int
ODR_clock_gettime(int clock_id, struct odr_timespec *tp)
{
	struct timespec tv;
	int id = -1;
	int ret;

	if ((clock_id & ODR_CLOCK_MONOTONIC) != 0)
		id = CLOCK_MONOTONIC;
	if ((clock_id & ODR_CLOCK_REALTIME) != 0)
		id = CLOCK_REALTIME;
	if ((clock_id & ODR_CLOCK_UPTIME) != 0)
		id = CLOCK_MONOTONIC;
	assert(id != -1);
	ret = clock_gettime(id, &tv);
	if (ret == -1)
		return (ret);
	tp->tv_sec = tv.tv_sec;
	tp->tv_nsec = tv.tv_nsec;
	return (ret);
}

int
ODR_traversal_dir(struct vtclog *vl, const char *path,
    int (*callback)(struct vtclog *vl, const char *name, void *arg), void *arg)
{
	struct dirent *de;
	DIR *d;
	int r;

	d = opendir(path);
	if (!d) {
		vtc_log(vl, 0, "BANDEC_00037: opendir() failed: %d %s",
		    errno, strerror(errno));
		return (-1);
	}
	while ((de = readdir(d))) {
		r = callback(vl, de->d_name, arg);
		if (r != 0)
			break;
	}
	closedir(d);
	return (0);
}

int
ODR_msleep(int miliseconds)
{

	return (usleep(miliseconds * 1000));
}

uint64_t
ODR_times(void)
{
	struct tms tms;

	return ((uint64_t)times(&tms));
}

void
ODR_bzero(void *buf, size_t len)
{

	memset(buf, 0, len);
}
