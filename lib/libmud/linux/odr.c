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

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
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
#include <pwd.h>
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

static const char *odr_homedir;

void
ODR_libinit(void)
{
	struct passwd *pwd;
	struct sigaction sac;
	int r;

	srand(time(NULL));

	memset(&sac, 0, sizeof sac);
	sac.sa_handler = SIG_IGN;
	sac.sa_flags = SA_RESTART;
	r = sigaction(SIGPIPE, &sac, NULL);
	assert(r == 0);

	pwd = getpwuid(getuid());
	AN(pwd);
	odr_homedir = ODR_strdup(pwd->pw_dir);
}

int
ODR_corefile_init(void)
{
	struct rlimit rlim;
	int r;

	/* Creates the core */
	ODR_bzero(&rlim, sizeof(rlim));
	rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
	r = setrlimit(RLIMIT_CORE, &rlim);
	if (r == -1)
		return (-errno);
	return (0);
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

char *
ODR_strdup(const char *str)
{

	return (strdup(str));
}

void
ODR_bcopy(const void *src, void *dst, size_t len)
{

	memcpy(dst, src, len);
}

void
ODR_bzero(void *buf, size_t len)
{

	memset(buf, 0, len);
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
ODR_n_errno(void)
{

	return (errno);
}

int
ODR_strncasecmp(const char *s1, const char *s2, size_t n)
{
	if (n != 0) {
		const uint8_t
				*us1 = (const uint8_t *)s1,
				*us2 = (const uint8_t *)s2;

		do {
			if (tolower(*us1) != tolower(*us2++))
				return (tolower(*us1) - tolower(*--us2));
			if (*us1++ == '\0')
				break;
		} while (--n != 0);
	}
	return (0);
}

int
ODR_snprintf(char *str, size_t size, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(str, size, fmt, ap);
	va_end(ap);
	return (n);
}

int
ODR_mkdir_recursive(const char *dir)
{
	char tmp[PATH_MAX];
	char *p = NULL;
	size_t len;
	int rv;

	snprintf(tmp, sizeof(tmp), "%s", dir);
	len = strlen(tmp);
	if (tmp[len - 1] == '/') {
		tmp[len - 1] = 0;
	}
	for (p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = 0;
			rv = mkdir(tmp, 0700);
			assert(rv == 0 || errno == EEXIST);
			*p = '/';
		}
	}
	rv = mkdir(tmp, 0700);
	assert(rv == 0 || errno == EEXIST);
	return (0);
}

const char *
ODR_homedir(void)
{

	return (odr_homedir);
}

int
ODR_msleep(int miliseconds)
{

	return (usleep(miliseconds * 1000));
}

int
ODR_errno(void)
{

	return (errno);
}

const char *
ODR_strerror(int errnum)
{

	return (strerror(errnum));
}

void
ODR_close(int fd)
{

	close(fd);
}

int
ODR_write(int d, const void *buf, size_t nbytes)
{

	return (write(d, buf, nbytes));
}

int
ODR_read(struct vtclog *vl, int d, void *buf, size_t nbytes)
{
	ssize_t r;

	r = read(d, buf, nbytes);
	if (r == -1)
		vtc_log(vl, 1, "BANDEC_00031: read(2) failed: %d %s", errno,
		    strerror(errno));
	return ((int)r);
}

uint64_t
ODR_times(void)
{
	struct tms tms;

	return ((uint64_t)times(&tms));
}

int
ODR_access(const char *path, int mode)
{

	if (mode == ODR_ACCESS_F_OK)
		mode = F_OK;
	else
		assert(0 == 1);
	return (access(path, mode));
}

int
ODR_unlink(const char *filename)
{

	return (unlink(filename));
}

ssize_t
ODR_recvfrom(struct vtclog *vl, int fd, void *buf, size_t len, int odr_flags,
    struct sockaddr *from, int *fromlen)
{
	ssize_t n;
	int flags = 0;

	if ((odr_flags & ODR_MSG_WAITALL) != 0)
		flags |= MSG_WAITALL;
	n = recvfrom(fd, buf, len, flags, from, (socklen_t *)fromlen);
	if (n == -1)
		vtc_log(vl, 1, "BANDEC_00032: recvfrom(2) failed: %d %s", errno,
		    strerror(errno));
	return (n);
}

const char *
ODR_confdir(void)
{
	const char *hdir;
	static char cdir[ODR_BUFSIZ];

	hdir = ODR_homedir();
	ODR_snprintf(cdir, sizeof(cdir), "%s/.config/mudband", hdir);
	return (cdir);
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
		vtc_log(vl, 0, "BANDEC_00033: opendir() failed: %d %s",
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

/*
 * Find the first occurrence of find in s, ignore case.
 */
const char *
ODR_strcasestr(const char *s, const char *find)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != 0) {
		c = tolower((unsigned char)c);
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == 0)
					return (NULL);
			} while ((char)tolower((unsigned char)sc) != c);
		} while (ODR_strncasecmp(s, find, len) != 0);
		s--;
	}
	return ((const char *)s);
}

int
ODR_flopen(const char *path, int flags, ...)
{
	int fd, operation, serrno, o_trunc;
	struct flock lock;
	struct stat sb, fsb;
	mode_t mode;

#ifdef O_EXLOCK
	flags &= ~O_EXLOCK;
#endif

	mode = 0;
	if (flags & O_CREAT) {
		va_list ap;

		va_start(ap, flags);
		mode = (mode_t)va_arg(ap, int); /* mode_t promoted to int */
		va_end(ap);
	}

	memset(&lock, 0, sizeof lock);
	lock.l_type = ((flags & O_ACCMODE) == O_RDONLY) ? F_RDLCK : F_WRLCK;
	lock.l_whence = SEEK_SET;
	operation = (flags & O_NONBLOCK) ? F_SETLK : F_SETLKW;

	o_trunc = (flags & O_TRUNC);
	flags &= ~O_TRUNC;

	for (;;) {
		if ((fd = open(path, flags, mode)) == -1)
			/* non-existent or no access */
			return (-1);
		if (fcntl(fd, operation, &lock) == -1) {
			/* unsupported or interrupted */
			serrno = errno;
			(void)close(fd);
			errno = serrno;
			return (-1);
		}
		if (stat(path, &sb) == -1) {
			/* disappeared from under our feet */
			(void)close(fd);
			continue;
		}
		if (fstat(fd, &fsb) == -1) {
			/* can't happen [tm] */
			serrno = errno;
			(void)close(fd);
			errno = serrno;
			return (-1);
		}
		if (sb.st_dev != fsb.st_dev ||
		    sb.st_ino != fsb.st_ino) {
			/* changed under our feet */
			(void)close(fd);
			continue;
		}
		if (o_trunc && ftruncate(fd, 0) != 0) {
			/* can't happen [tm] */
			serrno = errno;
			(void)close(fd);
			errno = serrno;
			return (-1);
		}
		return (fd);
	}
}
