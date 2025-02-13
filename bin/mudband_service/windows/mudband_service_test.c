/*-
 * Copyright (c) 2022 Weongyo Jeong <weongyo@3rocks.net>
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

#include <windows.h>
#include <accctrl.h>
#include <aclapi.h>
#include <sddl.h>
#include <stdarg.h>
#include <strsafe.h>

#include "odr.h"
#include "odr_pthread.h"
#include "vopt.h"
#include "vassert.h"
#include "vtc_log.h"

static struct vtclog *vl;

static const char *mudband_service_name = "\\\\.\\pipe\\mudband_service";

static int
serv_command_write(HANDLE handle, const char *cmd, int cmdlen)
{
	OVERLAPPED overlapped;
	memset(&overlapped, 0, sizeof(overlapped));
	overlapped.Offset = 0;
	overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (overlapped.hEvent == NULL) {
		vtc_log(vl, 0, "CreateFile() failed: %d",
		    GetLastError());
		return (-1);
	}
	DWORD written = 0;
	vtc_log(vl, 2, "Writing the command (len %d)", cmdlen);
	BOOL rv = WriteFile(handle, cmd, (DWORD)cmdlen, &written, &overlapped);
	if (rv != TRUE) {
		if (GetLastError() != ERROR_IO_PENDING) {
			CloseHandle(overlapped.hEvent);
			vtc_log(vl, 0, "WriteFile() failed: %d",
			    GetLastError());
			return (-1);
		}
		HANDLE events[] = { overlapped.hEvent };
		DWORD event_count = 1;
		DWORD wr = WaitForMultipleObjects(event_count, events, FALSE,
		    1000);
		if (wr != WAIT_OBJECT_0) {
			CloseHandle(overlapped.hEvent);
			vtc_log(vl, 0, "WaitForMultipleObjects() failed: %d",
			    GetLastError());
			return (-1);
		}
	}
	CloseHandle(overlapped.hEvent);
	return (0);
}

static DWORD
serv_command_read(HANDLE handle, uint8_t *data, DWORD data_size)
{
	OVERLAPPED overlapped;
	memset(&overlapped, 0, sizeof(overlapped));
	overlapped.Offset = 0;
	overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (overlapped.hEvent == NULL) {
		vtc_log(vl, 0, "CreateFile() failed: %d",
		    GetLastError());
		return (-1);
	}
	DWORD nread = 0;
	BOOL rv = ReadFile(handle, data, data_size, &nread, &overlapped);
	if (rv != TRUE) {
		if (GetLastError() != ERROR_IO_PENDING) {
			CloseHandle(overlapped.hEvent);
			vtc_log(vl, 0, "ReadFile() failed: %d",
			    GetLastError());
			return (-1);
		}
		HANDLE events[] = { overlapped.hEvent };
		DWORD event_count = 1;
		DWORD wr = WaitForMultipleObjects(event_count, events, FALSE,
		    1000);
		if (wr != WAIT_OBJECT_0) {
			CloseHandle(overlapped.hEvent);
			vtc_log(vl, 0, "WaitForMultipleObjects() failed: %d",
			    GetLastError());
			return (-1);
		}
		BOOL gor = GetOverlappedResult(handle, &overlapped, &nread,
		    FALSE);
		if (gor == FALSE) {
			CloseHandle(overlapped.hEvent);
			vtc_log(vl, 0, "GetOverlappedResult() failed: %d",
			    GetLastError());
			return (-1);
		}
	}
	CloseHandle(overlapped.hEvent);
	return (nread);
}

static int
serv_command(const char *cmd, int cmdlen)
{
	HANDLE handle;
	uint8_t resp[256];

	handle = CreateFile(mudband_service_name, GENERIC_READ | GENERIC_WRITE, 0,
	   NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		vtc_log(vl, 0, "CreateFile() failed: %d",
		    GetLastError());
		return (-1);
	}
	DWORD mode = PIPE_READMODE_MESSAGE;
	BOOL rv = SetNamedPipeHandleState(handle, &mode, NULL, NULL);
	if (!rv) {
		vtc_log(vl, 0, "SetNamedPipeHandleState() failed: %d",
		    GetLastError());
		return (-1);
	}
	int error = serv_command_write(handle, cmd, cmdlen);
	if (error) {
		vtc_log(vl, 0, "serv_command_write() failed: %d",
		    error);
		return (-1);
	}
	DWORD nread = serv_command_read(handle, resp, sizeof(resp));
	if (nread == -1) {
		vtc_log(vl, 0, "serv_command_write() failed: %d", error);
		return (-1);
	}
	vtc_log(vl, 2, "resp %.*s", nread, resp);
	return (0);
}

void
VAS_Fail(const char *func, const char *file, int line, const char *cond,
	    int xxx)
{

	vtc_log(vl, 0, "Critical! assert fail: %s %s:%d %s %d", func, file,
	    line, cond, xxx);
	abort();
}

int
main(int argc, char *argv[])
{
	int ch, i, buflen = 0;
	char buf[256];
	const char *c_arg = NULL;
	
	ODR_libinit();
	vtc_loginit();
	vl = vtc_logopen("ctl", NULL);
	assert(vl != NULL);

	while (-1 != (ch = VOPT_get(argc, argv, "c:"))) {
		switch (ch) {
		case 'c':
			c_arg = vopt_arg;
			break;
		default:
			break;
		}
	}
	argc -= vopt_ind;
	argv += vopt_ind;

	if (c_arg != NULL) {
		if (!strcmp(c_arg, "get_enrollment_count")) {
			buflen += snprintf(buf + buflen, sizeof(buf) - buflen,
			    "{ \"cmd\": \"get_enrollment_count\" }");
		}
	}
	for (i = 0; i < argc; i++) {
		buflen += snprintf(buf + buflen, sizeof(buf) - buflen, "%s",
		    argv[i]);
		if (i + 1 != argc) {
			buflen += snprintf(buf + buflen, sizeof(buf) - buflen,
			    " ");
		}
	}
	vtc_log(vl, 2, "request: %.*s", buflen, buf);
	return (serv_command(buf, buflen));
}
