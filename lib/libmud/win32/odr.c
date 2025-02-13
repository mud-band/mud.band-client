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

#ifdef _WIN32
#define __func__ __FUNCTION__
#pragma warning(disable: 4091)
#pragma warning(disable: 4996)
#define _CRT_RAND_S
#endif

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <shellapi.h>
#include <winhttp.h>
#include <winioctl.h>
#include <ws2tcpip.h>
#include <tchar.h>
#include <time.h>
#include <tlhelp32.h>
#include <dbghelp.h>
#include <shlobj.h>
#include <io.h>
#include <KnownFolders.h>
#include <ShlObj_core.h>

#include "odr.h"
#include "odr_pthread.h"
#include "vassert.h"
#include "vsb.h"
#include "vtc_log.h"
#include "vtim.h"

static char odr_homedir[MAX_PATH];

int
ODR_errno(void)
{

	return (GetLastError());
}

const char *
ODR_strerror(int dw)
{
	DWORD l;
	void *mbuf;
	static char buf[BUFSIZ];

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
	    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	    NULL, dw, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
	    (LPTSTR)&mbuf, 0, NULL);
	l = _snprintf(buf, sizeof(buf), "%s", (char *)mbuf);
	assert(l < sizeof(buf));
	LocalFree(mbuf);
	return (buf);
}

void
ODR_libinit(void)
{
	WSADATA wsaData;
	int ret;
	PWSTR programDataPath = NULL;
	char appdata_path[MAX_PATH];

	ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
	assert(ret == 0);
	ODR_pthread_process_attach();

	srand((unsigned int)time(NULL));

	if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_ProgramData, 0, NULL, &programDataPath))) {
		WideCharToMultiByte(CP_UTF8, 0, programDataPath, -1, 
			appdata_path, MAX_PATH, NULL, NULL);
		CoTaskMemFree(programDataPath);
		ODR_snprintf(odr_homedir, sizeof(odr_homedir),
		    "%s", appdata_path);
	} else {
		ODR_snprintf(odr_homedir, sizeof(odr_homedir),
		    "C:\\ProgramData");
	}
	ODR_mkdir_recursive(odr_homedir);
}

int
ODR_corefile_init(void)
{

	/* do nothing. */
	return (0);
}

void
ODR_bzero(void *buf, size_t len)
{

	memset(buf, 0, len);
}

void
ODR_bcopy(const void *src, void *dst, size_t len)
{

	memcpy(dst, src, len);
}

int
ODR_strncasecmp(const char *s1, const char *s2, size_t n)
{
	if (n != 0) {
		const u_char
				*us1 = (const u_char *)s1,
				*us2 = (const u_char *)s2;

		do {
			if (tolower(*us1) != tolower(*us2++))
				return (tolower(*us1) - tolower(*--us2));
			if (*us1++ == '\0')
				break;
		} while (--n != 0);
	}
	return (0);
}

char *
ODR_strdup(const char *str)
{

	return (_strdup(str));
}

void
ODR_TimeFormat(char *p, const char *fmt, double t)
{
	struct tm *tm;
	time_t tt;

	tt = (time_t)t;
	tm = gmtime(&tt);
	strftime(p, ODR_TIME_FORMAT_SIZE, fmt, tm);
}

double
ODR_real(void)
{
	struct odr_timespec ts;

	assert(ODR_clock_gettime(ODR_CLOCK_REALTIME, &ts) == 0);
	return (ts.tv_sec + 1e-9 * ts.tv_nsec);
}

double
ODR_trunc(double x)
{

	return ((x > 0) ? floor(x) : ceil(x));
}

int
ODR_clock_gettime(int clock_id, struct odr_timespec *tp)
{
#define	FILETIME_1970		0x019db1ded53e8000
#define	HECTONANOSEC_PER_SEC	10000000ui64
	ULARGE_INTEGER fti;
	LARGE_INTEGER count;
	LONGLONG remain;
	BOOL ret;
	static LARGE_INTEGER freq;
	static int inited = 0;

	switch (clock_id) {
	case ODR_CLOCK_REALTIME:
		GetSystemTimeAsFileTime((LPFILETIME)&fti);
		fti.QuadPart -= FILETIME_1970;
		tp->tv_sec = fti.QuadPart / HECTONANOSEC_PER_SEC;
		tp->tv_nsec = (uint64_t)(fti.QuadPart % HECTONANOSEC_PER_SEC) *
		    100;
		break;
	case ODR_CLOCK_MONOTONIC:
	case ODR_CLOCK_UPTIME:
		if (inited == 0) {
			ret = QueryPerformanceFrequency(&freq);
			assert(ret != 0);
			inited = 1;
		}
		ret = QueryPerformanceCounter(&count);
		assert(ret != 0);
		tp->tv_sec = (time_t)(count.QuadPart / freq.QuadPart);
		/* XXX correct? */
		remain = count.QuadPart % freq.QuadPart;
		remain *= 1000000000;
		remain /= freq.QuadPart;
		tp->tv_nsec = (uint64_t)remain;
		break;
	default:
		assert(0 == 1);
		return (-1);
	}
	return (0);
#undef HECTONANOSEC_PER_SEC
#undef FILETIME_1970
}

int
ODR_gettimeofday(struct odr_timeval *tp, void *tz)
{
	struct odr_timespec ts;

	assert(tz == NULL);
	assert(ODR_clock_gettime(ODR_CLOCK_REALTIME, &ts) == 0);
	tp->tv_sec = ts.tv_sec;
	tp->tv_usec = (long)(ts.tv_nsec / 1000);
	return (0);
}

int
ODR_n_errno(void)
{

	return (WSAGetLastError());
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

const char *
ODR_homedir(void)
{

	return (odr_homedir);
}

int
ODR_mkdir_recursive(const char *path)
{
	char tmppath[MAX_PATH];
	char* p = NULL;

	ODR_snprintf(tmppath, sizeof(tmppath), "%s", path);
	for (p = tmppath + 1; *p; p++) {
		if (*p != '\\' && *p != '/')
			continue;
		*p = '\0';
		if (!CreateDirectory(tmppath, NULL)) {
			if (GetLastError() != ERROR_ALREADY_EXISTS) {
				return (-1);
			}
		}
		*p = '\\';
	}
	if (!CreateDirectory(tmppath, NULL)) {
		if (GetLastError() != ERROR_ALREADY_EXISTS) {
			return (-1);
		}
	}
	return (0);
}

int
ODR_msleep(int miliseconds)
{

	Sleep(miliseconds);
	return (0);
}

void
ODR_close(int fd)
{

	closesocket(fd);
}

int
ODR_write(int d, const void *buf, size_t nbytes)
{
	int sent;

	sent = send(d, buf, (int)nbytes, 0);
	if (sent == SOCKET_ERROR)
		return (-1);
	assert(sent != SOCKET_ERROR);
	assert(sent == nbytes);
	return (sent);
}

uint64_t
ODR_times(void)
{
	ULONGLONG tick;

	/*
	 * XXX FIXME
	 * WG ODR_times() expected that the return type of this function is
	 * clock_t (on some system its type is 64 bits).  But at this moment
	 * I'm using GetTickCount which returns DWORD (32 bits) value because
	 * on my system GetTickCount64 doesn't exist.
	 */

	tick = GetTickCount64();
	return ((uint64_t)(tick / 10));
}

int
ODR_access(const char *path, int mode)
{

	if (mode == ODR_ACCESS_F_OK)
		mode = 00;
	else
		assert(0 == 1);
	return (_access(path, mode));
}

int
ODR_unlink(const char *filename)
{

	return (_unlink(filename));
}

int
ODR_read(struct vtclog *vl, int d, void *buf, size_t nbytes)
{
	int read;

	read = recv(d, buf, (int)nbytes, 0);
	if (read == 0)
		return (0);
	if (read == SOCKET_ERROR) {
		vtc_log(vl, 1,
		    "BANDEC_00028: recv(2) error: read %d error %ld",
		    read, WSAGetLastError());
		return (read);
	}
	assert(read > 0);
	return (read);
}

ssize_t
ODR_recvfrom(struct vtclog *vl, int fd, void *buf, size_t len, int odr_flags,
    struct sockaddr *from, int *fromlen)
{
	ssize_t n;
	int flags = 0;

	if ((odr_flags & ODR_MSG_WAITALL) != 0)
		flags |= MSG_WAITALL;
	n = recvfrom(fd, buf, len, flags, from, fromlen);
	if (n == SOCKET_ERROR) {
		vtc_log(vl, 1,
		    "BANDEC_00029: recvfrom(2) error: read %d error %ld",
		    n, WSAGetLastError());
		return (-1);
	}
	assert(n >= 0);
	return (n);
}

const char *
ODR_confdir(void)
{
	const char *hdir;
	static char cdir[ODR_BUFSIZ];

	hdir = ODR_homedir();
	ODR_snprintf(cdir, sizeof(cdir), "%s\\Mudfish Networks\\Mudband", hdir);
	return (cdir);
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
ODR_traversal_dir(struct vtclog *vl, const char *path,
    int (*callback)(struct vtclog *vl, const char *name, void *arg), void *arg)
{
	WIN32_FIND_DATAA ffd;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	int r = 0;
	char searchPath[MAX_PATH];

	snprintf(searchPath, sizeof(searchPath), "%s\\*", path);
	hFind = FindFirstFileA(searchPath, &ffd);
	if (hFind == INVALID_HANDLE_VALUE) {
		vtc_log(vl, 0, "BANDEC_00030: FindFirstFileA() failed: %d",
		    GetLastError());
		return (-1);
	}
	do {
		if (strcmp(ffd.cFileName, ".") == 0 ||
		    strcmp(ffd.cFileName, "..") == 0) {
			continue;
		}
		r = callback(vl, ffd.cFileName, arg);
		if (r != 0) {
			break;
		}
	} while (FindNextFileA(hFind, &ffd) != 0);
	FindClose(hFind);
	return (0);
}

int
ODR_strcasecmp(const char *s1, const char *s2)
{
	const u_char
			*us1 = (const u_char *)s1,
			*us2 = (const u_char *)s2;

	while (tolower(*us1) == tolower(*us2++))
		if (*us1++ == '\0')
			return (0);
	return (tolower(*us1) - tolower(*--us2));
}
