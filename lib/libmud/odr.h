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

#ifndef LIBMUD_ODR_H
#define	LIBMUD_ODR_H

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#if defined(_WIN32)
#include <windows.h>
#endif

/*
 * Notes that this error number is to solve a difference value of errno
 * on different OS (e.g. ETIMEDOUT of FreeBSD, 60 and linux, 110).
 */

#define	ODR_ETIMEDOUT	10000
#define	ODR_EINVAL	10001
#define	ODR_ENOSYS	10002
#define	ODR_ENOMEM	10003
#define	ODR_ENOSPC	10004
#define	ODR_EDEADLK	10005
#define	ODR_EPERM	10006
#define	ODR_EAGAIN	10007
#define	ODR_EBUSY	10008
#define	ODR_ERANGE	10009
#define	ODR_ESRCH	10010
#define	ODR_ENOENT	10011

/*--------------------------------------------------------------------------*/

#define	ODR_BUFSIZ	1024

/*--------------------------------------------------------------------------*/

/* Macros for min/max. */
#ifndef MIN
#define	MIN(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef MAX
#define	MAX(a,b) (((a)>(b))?(a):(b))
#endif

/* Assert zero return value */
#define AZ(foo)	do { assert((foo) == 0); } while (0)
#define AN(foo)	do { assert((foo) != 0); } while (0)

#if defined(_WIN32)
typedef SSIZE_T ssize_t;
#endif

/*--------------------------------------------------------------------------*/

struct odr_timespec {
	time_t			tv_sec;
	uint64_t		tv_nsec;
};

struct odr_timeval {
	time_t			tv_sec;
	long			tv_usec;
};

/*--------------------------------------------------------------------------*/

int	ODR_errno(void);
const char *
	ODR_strerror(int errnum);
void	ODR_libinit(void);
int	ODR_corefile_init(void);
void	ODR_bzero(void *buf, size_t len);
void	ODR_bcopy(const void *src, void *dst, size_t len);
int	ODR_strncasecmp(const char *s1, const char *s2, size_t n);
char *	ODR_strdup(const char *str);
#define	ODR_TIME_FORMAT_SIZE	30
void	ODR_TimeFormat(char *p, const char *fmt, double t);
double	ODR_real(void);
double	ODR_trunc(double x);
int	ODR_clock_gettime(int clock_id, struct odr_timespec *tp);
#define	ODR_CLOCK_MONOTONIC	(1 << 0)
#define	ODR_CLOCK_REALTIME	(1 << 1)
#define	ODR_CLOCK_UPTIME	(1 << 2)
int	ODR_gettimeofday(struct odr_timeval *tp, void *tz);
int	ODR_n_errno(void);
int	ODR_snprintf(char *str, size_t size, const char *fmt, ...);
const char *
	ODR_homedir(void);
const char *
	ODR_confdir(void);
int	ODR_mkdir_recursive(const char *dir);
int	ODR_msleep(int miliseconds);
void	ODR_close(int fd);
int	ODR_write(int d, const void *buf, size_t nbytes);
struct vtclog;
int	ODR_read(struct vtclog *vl, int d, void *buf, size_t nbytes);
uint64_t
	ODR_times(void);
#define	ODR_ACCESS_F_OK		00
int	ODR_access(const char *path, int mode);
int	ODR_unlink(const char *filename);
struct sockaddr;
#define	ODR_MSG_WAITALL		(1 << 0)
ssize_t	ODR_recvfrom(struct vtclog *vl, int fd, void *buf, size_t len,
	    int odr_flags, struct sockaddr *from, int *fromlen);
const char *
	ODR_strcasestr(const char *s, const char *find);
int	ODR_traversal_dir(struct vtclog *vl, const char *path,
	    int (*callback)(struct vtclog *vl, const char *name, void *arg),
	    void *arg);
int	ODR_flopen(const char *path, int flags, ...);
int	ODR_strcasecmp(const char *s1, const char *s2);

#endif /* !LIBMUD_ODR_H */
