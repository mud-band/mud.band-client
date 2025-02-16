/*-
 * Copyright (c) 2022 Weongyo Jeong <weongyo@gmail.com>
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

#include <accctrl.h>
#include <aclapi.h>
#include <direct.h>
#include <mbstring.h>
#include <processthreadsapi.h>
#include <sddl.h>
#include <shlwapi.h>
#include <stdarg.h>
#include <strsafe.h>
#include <tlhelp32.h>
#include <windows.h>

#include "jansson.h"
#include "odr.h"
#include "vassert.h"
#include "vhttps.h"
#include "vopt.h"
#include "vtc_log.h"

#include "crypto.h"
#include "wireguard.h"

#include "mudband_service.h"
#include "mudband_service_msg.h"

#define MUDBAND_SERVICE_NAME "mudband_service"
#define MUDBAND_SERVICE_DESC "Mud.band Service"

#define N(a) (sizeof(a) / sizeof((a)[0]))

#pragma comment(lib, "advapi32.lib")

struct tunnel_status {
	int is_running;
};
static struct tunnel_status band_tunnel_status;

char *band_confdir_enroll;
char *band_confdir_root;
struct vtclog *vl;

enum mudband_service_named_pipe_step {
	STEP_FIRST,
	STEP_READ,
	STEP_READ_WAIT,
	STEP_WRITE,
	STEP_WRITE_WAIT,
	STEP_ERROR,
	STEP_DONE
};

struct mudband_service_named_pipe {
	enum mudband_service_named_pipe_step step;
	HANDLE		pipe;
	OVERLAPPED	overlapped;
	char		buf_in[4096];
	DWORD		buf_in_len;
	char		buf_out[32 * 1024];
	DWORD		buf_out_len;
};

static SERVICE_STATUS_HANDLE mudband_status_handle;
static HANDLE mudband_service_stop_event;
static HANDLE mudband_process_handle = INVALID_HANDLE_VALUE;
static HANDLE mudband_thread_handle = INVALID_HANDLE_VALUE;
static const char *mudband_service_name = "\\\\.\\pipe\\mudband_service";
static int is_console_mode;

static int	svc_command_mudband_start();
static int	svc_command_mudband_stop(void);

uint32_t
wireguard_sys_now(void)
{
	struct odr_timespec ts;

	assert(ODR_clock_gettime(ODR_CLOCK_MONOTONIC, &ts) == 0);
	return (uint32_t)(ts.tv_sec * 1000L + ts.tv_nsec / 1000000L);
}

void
wireguard_random_bytes(void *bytes, size_t size)
{
	int x;
	uint8_t *out = (uint8_t *)bytes;

	for (x = 0; x < (int)size; x++) {
		out[x] = rand() % 0xFF;
	}
}

/*
 * See https://cr.yp.to/libtai/tai64.html
 *
 * 64 bit seconds from 1970 = 8 bytes
 * 32 bit nano seconds from current second.
 */
void
wireguard_tai64n_now(uint8_t *output)
{
	uint64_t millis, seconds;
	uint32_t nanos;

	millis = (uint64_t)wireguard_sys_now();
	/* Split into seconds offset + nanos */
	seconds = 0x400000000000000aULL + (millis / 1000);
	nanos = (millis % 1000) * 1000;
	U64TO8_BIG(output + 0, seconds);
	U32TO8_BIG(output + 8, nanos);
}

static int
svc_install(void)
{
	SC_HANDLE manager;
	SC_HANDLE service;
	CHAR unquoted_path[MAX_PATH];

	if (!GetModuleFileName(NULL, unquoted_path, MAX_PATH)) {
		fprintf(stderr,
		    "[ERROR] BANDEC_00639: Cannot install service (%d)\n",
		    GetLastError());
		return (-1);
	}
	manager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == manager) {
		fprintf(stderr,
		    "[ERROR] BANDEC_00640: OpenSCManager failed (%d)\n",
		    GetLastError());
		return (-1);
	}
	CHAR path[MAX_PATH];
	StringCbPrintf(path, MAX_PATH, "\"%s\"", unquoted_path);
	service = CreateService(manager, MUDBAND_SERVICE_NAME,
	    MUDBAND_SERVICE_DESC,
	    SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
	    SERVICE_ERROR_NORMAL, path, NULL, NULL, NULL, NULL, NULL);
	if (service == NULL) {
		int last_error;

		last_error = GetLastError();
		if (last_error == ERROR_SERVICE_EXISTS)
			return (0);
		fprintf(stderr,
		    "[ERROR] BANDEC_00641: CreateService failed (%d)\n",
		    last_error);
		CloseServiceHandle(manager);
		return (-1);
	} else
		fprintf(stdout,
		    "[INFO] Mud.band Service installed successfully.\n");
	CloseServiceHandle(service);
	CloseServiceHandle(manager);
	return (0);
}

static int
svc_uninstall(void)
{
	SC_HANDLE manager;
	SC_HANDLE service;
	CHAR unquoted_path[MAX_PATH];

	if (!GetModuleFileName(NULL, unquoted_path, MAX_PATH)) {
		fprintf(stderr,
		    "[ERROR] BANDEC_00642: Cannot install service (%d)\n",
		    GetLastError());
		return (-1);
	}
	manager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == manager) {
		fprintf(stderr,
		    "[ERROR] BANDEC_00643: OpenSCManager() failed (%d)\n",
		    GetLastError());
		return (-1);
	}
	CHAR path[MAX_PATH];
	StringCbPrintf(path, MAX_PATH, "\"%s\"", unquoted_path);
	service = OpenService(manager, MUDBAND_SERVICE_NAME, DELETE);
	if (service == NULL) {
		fprintf(stderr,
		    "[ERROR] BANDEC_00644: OpenService() failed (%d)\n",
		    GetLastError());
		CloseServiceHandle(manager);
		return (-1);
	}
	if (!DeleteService(service)) {
		fprintf(stderr,
		    "[ERROR] BANDEC_00645: DeleteService() failed (%d)\n",
		    GetLastError());
		CloseServiceHandle(service);
		CloseServiceHandle(manager);
		return (-1);
	}
	fprintf(stdout,
	    "[INFO] Mud.band Service uninstalled successfully.\n");
	CloseServiceHandle(service);
	CloseServiceHandle(manager);
	return (0);
}

static void
svc_set_status(DWORD current_state, DWORD exit_code, DWORD wait_hint)
{
	SERVICE_STATUS serv_status;

	static DWORD check_point = 1;
	memset(&serv_status, 0, sizeof(serv_status));
	serv_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	serv_status.dwCurrentState = current_state;
	serv_status.dwWin32ExitCode = exit_code;
	serv_status.dwWaitHint = wait_hint;
	if (current_state == SERVICE_START_PENDING)
		serv_status.dwControlsAccepted = 0;
	else
		serv_status.dwControlsAccepted =
		    SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SESSIONCHANGE;
	if ((current_state == SERVICE_RUNNING) ||
	    (current_state == SERVICE_STOPPED))
		serv_status.dwCheckPoint = 0;
	else
		serv_status.dwCheckPoint = check_point++;
	SetServiceStatus(mudband_status_handle, &serv_status);
}

static DWORD WINAPI
svc_handler(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData,
    LPVOID lpContext)
{

	switch (dwControl) {
	case SERVICE_CONTROL_STOP:
		svc_set_status(SERVICE_STOP_PENDING, NO_ERROR, 0);
		SetEvent(mudband_service_stop_event);
		return (NO_ERROR);
	case SERVICE_CONTROL_INTERROGATE:
		return (NO_ERROR);
	default:
		break;
	}
	return (ERROR_CALL_NOT_IMPLEMENTED);
}

static int
mudband_log_printf(const char *id, int lvl, double t_elapsed, const char *msg)
{
	HANDLE h;
	LPCSTR strs[2];
	int msgtype;
	char line[1024];

	if (is_console_mode)
		return (-1);

	snprintf(line, sizeof(line), "[%f] %-4s %s %s", t_elapsed,
	    id, vtc_lead(lvl), msg);
	switch (lvl) {
        case 0:
		msgtype = EVENTLOG_ERROR_TYPE;
		break;
        case 1:
		msgtype = EVENTLOG_WARNING_TYPE;
		break;
        case 2:
		msgtype = EVENTLOG_INFORMATION_TYPE;
		break;
        case 3:
        default:
		msgtype = EVENTLOG_INFORMATION_TYPE;
		break;
	}

	h = RegisterEventSource(NULL, MUDBAND_SERVICE_NAME);
	if (h == NULL) {
		fprintf(stderr,
		    "[ERROR] BANDEC_00646: RegisterEventSource() failed");
		return (1);
	}
	strs[0] = MUDBAND_SERVICE_DESC;
	strs[1] = line;
	BOOL r = ReportEvent(h, msgtype, 0, SVC_ERROR, NULL, 2, 0, strs, NULL);
	if (!r) {
		fprintf(stderr,
		    "[WARN] BANDEC_00647: ReportEvent() failed: %d\n",
		    GetLastError());
	}
	(void)DeregisterEventSource(h);

	return (1);
}

void
VAS_Fail(const char *func, const char *file, int line, const char *cond,
	    int xxx)
{

	vtc_log(vl, 0, "Critical! assert fail: %s %s:%d %s %d", func, file,
	    line, cond, xxx);
	abort();
}

static struct mudband_service_named_pipe *
svc_named_pipe_server_create(void)
{
	struct mudband_service_named_pipe *p;
	EXPLICIT_ACCESS ec[2];
	PSECURITY_DESCRIPTOR sd;
	PACL acl_new, acl_odl;
	PSID sid_anonymous, sid_everyone;
	DWORD rv;

	p = (struct mudband_service_named_pipe *)calloc(1, sizeof(*p));
	if (p == NULL) {
		vtc_log(vl, 0, "BANDEC_00648: OOM");
		return (NULL);
	}
	p->step = STEP_FIRST;
	p->pipe = CreateNamedPipe(mudband_service_name,
	    PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | WRITE_DAC,
	    PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
	    PIPE_UNLIMITED_INSTANCES, sizeof(p->buf_out),
	    sizeof(p->buf_in), 5000, NULL);
	if (p->pipe == INVALID_HANDLE_VALUE) {
		vtc_log(vl, 0, "BANDEC_00649: CreateNamedPipe() failed.");
		free(p);
		return (NULL);
	}
	if (!ConvertStringSidToSid(SDDL_EVERYONE, &sid_everyone)) {
		vtc_log(vl, 0,
		    "BANDEC_00650: ConvertStringSidToSid for everyone");
		goto fail;
	}
	if (!ConvertStringSidToSid(SDDL_ANONYMOUS, &sid_anonymous)) {
		vtc_log(vl, 0,
		    "BANDEC_00651: ConvertStringSidToSid for anonymous");
		goto fail;
	}
	ec[0].grfAccessPermissions = FILE_GENERIC_WRITE;
	ec[0].grfAccessMode = GRANT_ACCESS;
	ec[0].grfInheritance = NO_INHERITANCE;
	ec[0].Trustee.pMultipleTrustee = NULL;
	ec[0].Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
	ec[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
	ec[0].Trustee.TrusteeType = TRUSTEE_IS_UNKNOWN;
	ec[0].Trustee.ptstrName = (LPTSTR)sid_everyone;
	ec[1].grfAccessPermissions = 0;
	ec[1].grfAccessMode = REVOKE_ACCESS;
	ec[1].grfInheritance = NO_INHERITANCE;
	ec[1].Trustee.pMultipleTrustee = NULL;
	ec[1].Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
	ec[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
	ec[1].Trustee.TrusteeType = TRUSTEE_IS_UNKNOWN;
	ec[1].Trustee.ptstrName = (LPTSTR)sid_anonymous;
	rv = GetSecurityInfo(p->pipe, SE_KERNEL_OBJECT,
	    DACL_SECURITY_INFORMATION, NULL, NULL, &acl_odl, NULL, &sd);
	if (rv != ERROR_SUCCESS) {
		vtc_log(vl, 0, "BANDEC_00652: GetSecurityInfo");
		goto fail;
	}
	rv = SetEntriesInAcl(2, ec, acl_odl, &acl_new);
	if (rv != ERROR_SUCCESS) {
		vtc_log(vl, 0, "BANDEC_00653: SetEntriesInAcl");
		goto fail;
	}
	rv = SetSecurityInfo(p->pipe, SE_KERNEL_OBJECT,
	    DACL_SECURITY_INFORMATION, NULL, NULL, acl_new, NULL);
	if (rv != ERROR_SUCCESS) {
		vtc_log(vl, 0, "BANDEC_00654: SetSecurityInfo");
		goto fail;
	}
	p->overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE,
	    NULL);
	if (p->overlapped.hEvent == NULL)
		goto fail;
	return (p);
fail:
	CloseHandle(p->pipe);
	free(p);
	return (NULL);
}

static void
svc_reset_overlapped(struct mudband_service_named_pipe *p)
{
	HANDLE event = p->overlapped.hEvent;

	ResetEvent(event);
	memset(&p->overlapped, 0, sizeof(p->overlapped));
	p->overlapped.hEvent = event;
}

static void
svc_named_pipe_server_connect(struct mudband_service_named_pipe *p)
{
	BOOL connected;

	svc_reset_overlapped(p);
	connected = ConnectNamedPipe(p->pipe, &p->overlapped);
	if (!connected) {
		switch (GetLastError()) {
		case ERROR_IO_PENDING:
			break;
		case ERROR_PIPE_CONNECTED:
			(void)SetEvent(p->overlapped.hEvent);
			break;
		default:
			vtc_log(vl, 0, "BANDEC_00655: ConnectNamedPipe");
			break;
		}
	}
	p->step = STEP_READ;
}

static int
svc_set_resp(struct mudband_service_named_pipe *p, int status,
    const char *msg)
{
	json_t *root;
	char *json_str;

	root = json_object();
	AN(root);
	json_object_set_new(root, "status", json_integer(status));
	json_object_set_new(root, "msg", json_string(msg));
	json_str = json_dumps(root, 0);
	AN(json_str);
	p->buf_out_len = snprintf(p->buf_out, sizeof(p->buf_out), "%s", json_str);
	free(json_str);
	json_decref(root);
	return (1);
}

static int
svc_check_process(const char *progname)
{
	HANDLE snapshot;
	PROCESSENTRY32 entry;
	int count = 0;

	entry.dwSize = sizeof(PROCESSENTRY32);
	snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) {
		vtc_log(vl, 0,
		    "BANDEC_00656: CreateToolhelp32Snapshot() error: %s",
		    GetLastError());
		return (-1);
	}
	if (Process32First(snapshot, &entry) != TRUE) {
		CloseHandle(snapshot);
		vtc_log(vl, 0, "BANDEC_00657: Process32First() error: %s",
		    GetLastError());
		return (-1);
	}

	while (Process32Next(snapshot, &entry) == TRUE)
		if (strcmp(entry.szExeFile, progname) == 0)
			count++;
	CloseHandle(snapshot);
	return (count);
}

static int
svc_kill_process(const char *pname)
{
	HANDLE snapshot, hProcess;
	PROCESSENTRY32 entry;

	entry.dwSize = sizeof(PROCESSENTRY32);
	snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) {
		vtc_log(vl, 0,
		    "BANDEC_00658: CreateToolhelp32Snapshot() error: %s",
		    GetLastError());
		return (-1);
	}
	if (Process32First(snapshot, &entry) != TRUE) {
		CloseHandle(snapshot);
		vtc_log(vl, 0, "BANDEC_00659: Process32First() error: %s",
		    GetLastError());
		return (-1);
	}
	while (Process32Next(snapshot, &entry) == TRUE) {
		if (strcmp(entry.szExeFile, pname))
			continue;
		hProcess = OpenProcess(PROCESS_TERMINATE, FALSE,
			entry.th32ProcessID);
		if (hProcess == NULL)
			continue;
		TerminateProcess(hProcess, 1);
		CloseHandle(hProcess);
	}
	CloseHandle(snapshot);
	return (0);
}

static int
svc_command_mudband_stop(void)
{
	int rv = 0;

	if (svc_kill_process("mudband.exe"))
		rv = -1;
	return (rv);
}

static ssize_t
cmd_enroll(char *out, size_t outmax, json_t *root)
{
	json_t *args, *enrollment_token, *device_name, *enrollment_secret;

	args = json_object_get(root, "args");
	AN(args);
	assert(json_is_object(args));
	enrollment_token = json_object_get(args, "enrollment_token");
	AN(enrollment_token);
	assert(json_is_string(enrollment_token));
	device_name = json_object_get(args, "device_name");
	AN(device_name);
	assert(json_is_string(device_name));
	enrollment_secret = json_object_get(args, "enrollment_secret");
	AN(enrollment_secret);
	assert(json_is_string(enrollment_secret) || json_is_null(enrollment_secret));

	return (MBE_enroll(out, outmax, json_string_value(enrollment_token),
	    json_string_value(device_name),
	    json_is_null(enrollment_secret) ? "" : json_string_value(enrollment_secret)));
}

static ssize_t
cmd_unenroll(char *out, size_t outmax, json_t *root)
{
	json_t *args, *band_uuid, *response;
	ssize_t outlen = 0;
	char *p;
	int rv;

	args = json_object_get(root, "args");
	if (!args || !json_is_object(args)) {
		vtc_log(vl, VTCLOG_LEVEL_ERROR, 
		    "BANDEC_00660: Invalid arguments for unenroll");
		return (-1);
	}

	band_uuid = json_object_get(args, "band_uuid");
	if (!band_uuid || !json_is_string(band_uuid)) {
		vtc_log(vl, VTCLOG_LEVEL_ERROR, 
		    "BANDEC_00661: Missing or invalid band UUID");
		return (-1);
	}

	response = json_object();
	AN(response);
	rv = MBE_unenroll(json_string_value(band_uuid));
	assert(rv == 0);
	json_object_set_new(response, "status", json_integer(200));
	json_object_set_new(response, "msg",
	    json_string("Successfully unenrolled"));

	p = json_dumps(response, 0);
	AN(p);
	outlen = snprintf(out, outmax, "%s", p);
	free(p);
	json_decref(response);

	return (outlen);
}

static ssize_t
cmd_get_active_band(char *out, size_t outmax)
{
	json_t *root, *active_band;
	ssize_t outlen = 0;
	char *p;

	root = json_object();
	assert(root != NULL);

	active_band = MBE_get_active_band();
	if (active_band == NULL) {
		json_object_set_new(root, "status", json_integer(500));
		json_object_set_new(root, "msg",
		    json_string("Failed to get active band information"));
	} else {
		json_object_set_new(root, "status", json_integer(200));
		json_object_set_new(root, "band", active_band);
	}
	p = json_dumps(root, 0);
	assert(p != NULL);
	outlen = snprintf(out, outmax, "%s", p);
	free(p);
	json_decref(root);
	return (outlen);
}

static ssize_t
cmd_get_enrollment_list(char *out, size_t outmax)
{
	json_t *root, *enrollment_list;
	ssize_t outlen = 0;
	char *p;

	root = json_object();
	assert(root != NULL);

	enrollment_list = MBE_get_enrollment_list();
	if (enrollment_list == NULL) {
		json_object_set_new(root, "status", json_integer(500));
		json_object_set_new(root, "msg",
		    json_string("Failed to get enrollment list"));
	} else {
		json_object_set_new(root, "status", json_integer(200));
		json_object_set_new(root, "enrollments", enrollment_list);
	}

	p = json_dumps(root, 0);
	assert(p != NULL);
	outlen = snprintf(out, outmax, "%s", p);
	free(p);
	json_decref(root);
	return (outlen);
}

static ssize_t
cmd_change_enrollment(char *out, size_t outmax, json_t *root)
{
	json_t *args, *band_uuid;
	ssize_t outlen = 0;
	json_t *response;
	char *p;

	args = json_object_get(root, "args");
	if (!args || !json_is_object(args)) {
		vtc_log(vl, VTCLOG_LEVEL_ERROR, 
			"BANDEC_00662: Invalid arguments for change_enrollment");
		return (-1);
	}

	band_uuid = json_object_get(args, "band_uuid");

	if (!band_uuid || !json_is_string(band_uuid)) {
		vtc_log(vl, VTCLOG_LEVEL_ERROR, 
			"BANDEC_00663: Missing or invalid band UUID.");
		return (-1);
	}

	response = json_object();
	AN(response);

	MPC_set_default_band_uuid(json_string_value(band_uuid));
	json_object_set_new(response, "status", json_integer(200));
	json_object_set_new(response, "msg", 
		json_string("Enrollment changed successfully"));

	p = json_dumps(response, 0);
	AN(p);
	outlen = snprintf(out, outmax, "%s", p);
	free(p);
	json_decref(response);

	return (outlen);
}

static ssize_t
cmd_get_active_conf(char *out, size_t outmax)
{
	json_t *root, *active_conf;
	ssize_t outlen = 0;
	char *p;

	root = json_object();
	assert(root != NULL);
	
	active_conf = CNF_get_active_conf();
	if (active_conf == NULL) {
		json_object_set_new(root, "status", json_integer(500));
		json_object_set_new(root, "msg",
		    json_string("No config found.  Please connect first."));
	} else {
		assert(json_is_object(active_conf));
		json_object_set_new(root, "status", json_integer(200));
		json_object_set_new(root, "conf", active_conf);
	}
	p = json_dumps(root, 0);
	assert(p != NULL);
	outlen = snprintf(out, outmax, "%s", p);
	free(p);
	json_decref(root);
	return (outlen);
}

static ssize_t
cmd_ping(char *out, size_t outmax)
{
	json_t *root;
	ssize_t outlen = 0;
	char *p;

	root = json_object();
	assert(root != NULL);
	json_object_set_new(root, "status", json_integer(200));
	json_object_set_new(root, "msg", json_string("pong"));
	p = json_dumps(root, 0);
	assert(p != NULL);
	outlen = snprintf(out, outmax, "%s", p);
	free(p);
	json_decref(root);
	return (outlen);
}

static ssize_t
cmd_tunnel_get_status(char *out, size_t outmax)
{
	json_t *root;
	ssize_t outlen = 0;
	char *p;

	root = json_object();
	assert(root != NULL);
	json_object_set_new(root, "status", json_integer(200));
	json_object_set_new(root, "tunnel_is_running",
	    json_boolean(band_tunnel_status.is_running));
	p = json_dumps(root, 0);
	assert(p != NULL);
	outlen = snprintf(out, outmax, "%s", p);
	free(p);
	json_decref(root);
	return (outlen);
}

static ssize_t
cmd_get_enrollment_count(char *out, size_t outmax)
{
	json_t *root;
	ssize_t outlen = 0;
	int enrollment_count;
	char *p;

	root = json_object();
	assert(root != NULL);
	enrollment_count = MBE_get_enrollment_count();
	if (enrollment_count == -1) {
		json_object_set_new(root, "status", json_integer(500));
		json_object_set_new(root, "msg",
		    json_string("BANDEC_00664: MBE_get_enrollment_count() failed"));
	} else {
		json_object_set_new(root, "status", json_integer(200));
		json_object_set_new(root, "enrollment_count",
		    json_integer(enrollment_count));
	}
	p = json_dumps(root, 0);
	assert(p != NULL);
	outlen = snprintf(out, outmax, "%s", p);
	free(p);
	json_decref(root);
	return (outlen);
}

static ssize_t
cmd_tunnel_connect(char *out, size_t outmax)
{
	json_t *root;
	ssize_t outlen = 0;
	int rv;
	char *p;

	root = json_object();
	assert(root != NULL);

	if (band_tunnel_status.is_running) {
		json_object_set_new(root, "status", json_integer(200));
		json_object_set_new(root, "msg", 
			json_string("Tunnel is already running"));
	} else {
		rv = svc_command_mudband_stop();
		if (rv == -1) {
			json_object_set_new(root, "status", json_integer(500));
			json_object_set_new(root, "msg", 
				json_string("Failed to stop tunnel"));
			goto out;
		}
		rv = svc_command_mudband_start();
		if (rv == -1) {
			json_object_set_new(root, "status", json_integer(501));
			json_object_set_new(root, "msg", 
				json_string("Failed to start tunnel"));
			goto out;
		}
		band_tunnel_status.is_running = 1;
		json_object_set_new(root, "status", json_integer(200));
		json_object_set_new(root, "msg", 
		    json_string("Tunnel started successfully"));
	}
out:
	p = json_dumps(root, 0);
	assert(p != NULL);
	outlen = snprintf(out, outmax, "%s", p);
	free(p);
	json_decref(root);
	return (outlen);
}

static ssize_t
cmd_tunnel_disconnect(char *out, size_t outmax)
{
	json_t *root;
	ssize_t outlen = 0;
	char *p;
	int rv;

	root = json_object();
	assert(root != NULL);

	if (!band_tunnel_status.is_running) {
		json_object_set_new(root, "status", json_integer(200));
		json_object_set_new(root, "msg", 
			json_string("Tunnel is not running"));
	} else {
		rv = svc_command_mudband_stop();
		if (rv == -1) {
			json_object_set_new(root, "status", json_integer(500));
			json_object_set_new(root, "msg", 
			    json_string("Failed to stop tunnel"));
			goto out;
		}
		band_tunnel_status.is_running = 0;
		json_object_set_new(root, "status", json_integer(200));
		json_object_set_new(root, "msg", 
		    json_string("Tunnel stopped successfully"));
	}
out:
	p = json_dumps(root, 0);
	assert(p != NULL);
	outlen = snprintf(out, outmax, "%s", p);
	free(p);
	json_decref(root);
	return (outlen);
}

static DWORD
svc_named_pipe_server_read(struct mudband_service_named_pipe *p)
{
	json_t *root, *cmd;
	json_error_t error;
	const char *cmdval;

	assert(p->buf_in_len < sizeof(p->buf_in));
	if (p->buf_in_len <= 0)
		return (svc_set_resp(p, 400, "short in buffer."));
	p->buf_in[p->buf_in_len] = '\0';
	root = json_loads(p->buf_in, 0, &error);
	if (root == NULL)
		return (svc_set_resp(p, 401, error.text));
	if (!json_is_object(root)) {
		json_decref(root);
		return (svc_set_resp(p, 402, "not object."));
	}
	cmd = json_object_get(root, "cmd");
	if (cmd == NULL || !json_is_string(cmd)) {
		json_decref(root);
		return (svc_set_resp(p, 403,
		    "command object issue."));	
	}
	cmdval = json_string_value(cmd);
	if (strcmp(cmdval, "enroll") == 0) {
		p->buf_out_len = cmd_enroll(p->buf_out, sizeof(p->buf_out),
		    root);
	} else if (strcmp(cmdval, "unenroll") == 0) {
		p->buf_out_len = cmd_unenroll(p->buf_out, sizeof(p->buf_out),
		    root);
	} else if (strcmp(cmdval, "get_active_band") == 0) {
		p->buf_out_len = cmd_get_active_band(p->buf_out,
		    sizeof(p->buf_out));
	} else if (strcmp(cmdval, "get_active_conf") == 0) {
		p->buf_out_len = cmd_get_active_conf(p->buf_out,
		    sizeof(p->buf_out));
	} else if (strcmp(cmdval, "get_enrollment_count") == 0) {
		p->buf_out_len = cmd_get_enrollment_count(p->buf_out,
		    sizeof(p->buf_out));
	} else if (strcmp(cmdval, "ping") == 0) {
		p->buf_out_len = cmd_ping(p->buf_out, sizeof(p->buf_out));
	} else if (strcmp(cmdval, "tunnel_get_status") == 0) {
		p->buf_out_len = cmd_tunnel_get_status(p->buf_out,
		    sizeof(p->buf_out));
	} else if (strcmp(cmdval, "tunnel_connect") == 0) {
		p->buf_out_len = cmd_tunnel_connect(p->buf_out,
		    sizeof(p->buf_out));
	} else if (strcmp(cmdval, "tunnel_disconnect") == 0) {		
		p->buf_out_len = cmd_tunnel_disconnect(p->buf_out,
		    sizeof(p->buf_out));
	} else if (strcmp(cmdval, "get_enrollment_list") == 0) {
		p->buf_out_len = cmd_get_enrollment_list(p->buf_out,
		    sizeof(p->buf_out));
	} else if (strcmp(cmdval, "change_enrollment") == 0) {
		p->buf_out_len = cmd_change_enrollment(p->buf_out,
		    sizeof(p->buf_out), root);
	} else {
		vtc_log(vl, 0, "BANDEC_00665: Unknown command: %s",
		    json_string_value(cmd));
		return (svc_set_resp(p, 404, "unknown command."));	
	}
	json_decref(root);
	return (1);
}

static void
svc_named_pipe_server_event(struct mudband_service_named_pipe *named_pipe)
{
	DWORD rv;
	BOOL done = FALSE, success;

	success = GetOverlappedResult(named_pipe->pipe, &named_pipe->overlapped,
	    &rv, FALSE);
	if (!success) {
		vtc_log(vl, 0, "BANDEC_00666: GetOverlappedResult");
		return;
	}
	while (!done) {
		switch (named_pipe->step) {
		case STEP_READ:
			svc_reset_overlapped(named_pipe);
			success = ReadFile(named_pipe->pipe, named_pipe->buf_in,
			    sizeof(named_pipe->buf_in), &named_pipe->buf_in_len,
			    &named_pipe->overlapped);
			if (!success) {
				switch (GetLastError()) {
				case ERROR_IO_PENDING:
					named_pipe->step = STEP_READ_WAIT;
					return;
				default:
					vtc_log(vl, 0,
					    "BANDEC_00667: ReadFile");
					named_pipe->step = STEP_ERROR;
					continue;
				}
			}
			rv = svc_named_pipe_server_read(named_pipe);
			if (rv) {
				named_pipe->step = STEP_WRITE;
			}
			break;
		case STEP_READ_WAIT:
			named_pipe->buf_in_len = rv;
			rv = svc_named_pipe_server_read(named_pipe);
			if (rv) {
				named_pipe->step = STEP_WRITE;
			}
			break;
		case STEP_WRITE:
			svc_reset_overlapped(named_pipe);
			success = WriteFile(named_pipe->pipe,
			    named_pipe->buf_out, named_pipe->buf_out_len, &rv,
			    &named_pipe->overlapped);
			if (!success) {
				switch (GetLastError()) {
				case ERROR_IO_PENDING:
					named_pipe->step = STEP_WRITE_WAIT;
					return;
				default:
					vtc_log(vl, 0,
					    "BANDEC_00668: WriteFile");
					named_pipe->step = STEP_ERROR;
					continue;
				}
			}
			if (rv != named_pipe->buf_out_len) {
				vtc_log(vl, 0, "BANDEC_00669: short write");
				named_pipe->step = STEP_ERROR;
				continue;
			}
			named_pipe->step = STEP_DONE;
			break;
		case STEP_WRITE_WAIT:
			if (rv != named_pipe->buf_out_len) {
				vtc_log(vl, 0, "BANDEC_00670: short write");
				named_pipe->step = STEP_ERROR;
				continue;
			}
			named_pipe->step = STEP_DONE;
			break;
		case STEP_ERROR:
			named_pipe->step = STEP_DONE;
			break;
		case STEP_DONE:
			if (!DisconnectNamedPipe(named_pipe->pipe)) {
				vtc_log(vl, 0,
				    "BANDEC_00671: DisconnectNamedPipe");
			}
			svc_named_pipe_server_connect(named_pipe);
			done = TRUE;
			break;
		}
	}
}

static int
svc_execute_mudband(char *path)
{
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	char cmd[ODR_BUFSIZ];

	// Add quotes around the path to handle spaces
	snprintf(cmd, sizeof(cmd), "\"%s\"", path);
	
	vtc_log(vl, 2, "Running the command: %s", cmd);

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW,
            NULL, NULL, &si, &pi)) {
		vtc_log(vl, 0, "BANDEC_00672: CreateProcess() failed: %d",
		    GetLastError());
		return (-1);
	}
	mudband_process_handle = pi.hProcess;
	mudband_thread_handle = pi.hThread;
	return (0);
}

static int
svc_command_mudband_start(void)
{
	char path[ODR_BUFSIZ];
	BOOL success;

	DWORD pathlen = GetModuleFileNameA(NULL, path, sizeof(path));
	assert(pathlen < sizeof(path));
	assert(pathlen != 0);
	success = PathRemoveFileSpecA(path);
	assert(success);
	success = PathAppendA(path, "mudband.exe");
	assert(success);
	return (svc_execute_mudband(path));
}

static void
svc_chdir(void)
{
	char path[ODR_BUFSIZ];
	BOOL success;

	DWORD pathlen = GetModuleFileNameA(NULL, path, sizeof(path));
	assert(pathlen < sizeof(path));
	assert(pathlen != 0);
	success = PathRemoveFileSpecA(path);
	assert(success);
	_chdir(path);
}

static void
check_tunnel_status(void)
{
	int is_running;
	
	is_running = svc_check_process("mudband.exe");
	if (is_running < 0) {
		vtc_log(vl, VTCLOG_LEVEL_ERROR,
			"BANDEC_00673: Failed to check running process");
		return;
	}
	if (is_running > 0) {
		if (band_tunnel_status.is_running == 0) {
			vtc_log(vl, VTCLOG_LEVEL_INFO,
			    "Changed tunnel status: stopped -> running");
		}
		band_tunnel_status.is_running = 1;
	} else {
		if (band_tunnel_status.is_running == 1) {
			vtc_log(vl, VTCLOG_LEVEL_INFO,
			    "Changed tunnel status: running -> stopped");
		}
		band_tunnel_status.is_running = 0;
	}
}

static void
svc_init(void)
{
	const char *cdir;
	char edir[ODR_BUFSIZ];

	ODR_libinit();
	vtc_loginit();
	vl = vtc_logopen("srv", mudband_log_printf);
	assert(vl != NULL);
	VHTTPS_init();
	svc_chdir();

	cdir = ODR_confdir();
	ODR_mkdir_recursive(cdir);
	band_confdir_root = ODR_strdup(cdir);
	AN(band_confdir_root);
	ODR_snprintf(edir, sizeof(edir), "%s\\enroll", band_confdir_root);
	ODR_mkdir_recursive(edir);
	band_confdir_enroll = ODR_strdup(edir);
	AN(band_confdir_enroll);

	MPC_init();
	check_tunnel_status();
}

static void WINAPI
svc_main(DWORD argc, LPTSTR *argv)
{
	struct mudband_service_named_pipe *p;
	HANDLE events[3];
	int n_events;

	mudband_status_handle = RegisterServiceCtrlHandlerEx(MUDBAND_SERVICE_NAME,
	    svc_handler, NULL);
	if (!mudband_status_handle) {
		vtc_log(vl, 0, "BANDEC_00674: RegisterServiceCtrlHandler");
		return;
	}
	svc_set_status(SERVICE_START_PENDING, NO_ERROR, 3000);
	mudband_service_stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (mudband_service_stop_event == NULL) {
		svc_set_status(SERVICE_STOPPED, GetLastError(), 0);
		return;
	}
	svc_init();
	p = svc_named_pipe_server_create();
	if (p == NULL) {
		svc_set_status(SERVICE_STOPPED, -1, 0);
		return;
	}
	svc_set_status(SERVICE_RUNNING, NO_ERROR, 0);
	svc_named_pipe_server_connect(p);
	while (1) {
		n_events = 2;
		events[0] = mudband_service_stop_event;
		events[1] = p->overlapped.hEvent;
		if (mudband_process_handle != INVALID_HANDLE_VALUE) {
			n_events = 3;
			events[2] = mudband_process_handle;
		}
		DWORD rv = WaitForMultipleObjects(n_events, events, FALSE,
		    1000);
		if (rv < WAIT_OBJECT_0 || rv > WAIT_OBJECT_0 + n_events) {
			if (rv == WAIT_TIMEOUT) {
				continue;
			}
			vtc_log(vl, 0,
			    "BANDEC_00675: WaitForMultipleObjects() failed: %d",
			    rv);
			break;
		}
		if (rv == WAIT_OBJECT_0)
			break;
		if (rv == WAIT_OBJECT_0 + 1) {
			svc_named_pipe_server_event(p);
			continue;
		}
		if (n_events == 3 && rv == WAIT_OBJECT_0 + 2) {
			DWORD exit_code = 0;

			GetExitCodeProcess(mudband_process_handle, &exit_code);
			CloseHandle(mudband_process_handle);
			CloseHandle(mudband_thread_handle);
			mudband_process_handle = INVALID_HANDLE_VALUE;
			mudband_thread_handle = INVALID_HANDLE_VALUE;
			continue;
		}
		vtc_log(vl, 0, "BANDEC_00676: Unexpected rv from"
		    " WaitForMultipleObjects()");
	}
	svc_set_status(SERVICE_STOPPED, NO_ERROR, 0);
}

static void
console_main(void)
{
	struct mudband_service_named_pipe *p;
	HANDLE events[2];
	int n_events;
	BOOL running = TRUE;

	is_console_mode = 1;

	svc_init();
	p = svc_named_pipe_server_create();
	if (p == NULL) {
		vtc_log(vl, 0, "BANDEC_00678: Failed to create named pipe server");
		return;
	}

	vtc_log(vl, VTCLOG_LEVEL_INFO, "mudband service started.");

	svc_named_pipe_server_connect(p);
	
	while (running) {
		n_events = 1;
		events[0] = p->overlapped.hEvent;
		if (mudband_process_handle != INVALID_HANDLE_VALUE) {
			n_events = 2;
			events[1] = mudband_process_handle;
		}

		DWORD rv = WaitForMultipleObjects(n_events, events, FALSE, 1000);
		if (rv < WAIT_OBJECT_0 || rv > WAIT_OBJECT_0 + n_events) {
			if (rv == WAIT_TIMEOUT) {
				continue;
			}
			vtc_log(vl, 0, 
				"BANDEC_00706: WaitForMultipleObjects() failed: %d",
				rv);
			break;
		}

		if (rv == WAIT_OBJECT_0) {
			svc_named_pipe_server_event(p);
			continue;
		}

		if (n_events == 2 && rv == WAIT_OBJECT_0 + 1) {
			DWORD exit_code = 0;

			GetExitCodeProcess(mudband_process_handle, &exit_code);
			CloseHandle(mudband_process_handle);
			CloseHandle(mudband_thread_handle);
			mudband_process_handle = INVALID_HANDLE_VALUE;
			mudband_thread_handle = INVALID_HANDLE_VALUE;
			continue;
		}

		vtc_log(vl, 0, "BANDEC_00707: Unexpected rv from WaitForMultipleObjects()");
	}
}

static void
usage(void)
{
	printf("Usage: mudband_service <cmds>\n");
	printf("Commands:\n");
	printf("  install         Install mudband_service service.\n");
	printf("  uninstall       Uninstall mudband_service service.\n");
	exit(0);
}

int
main(int argc, char *argv[])
{
	char svcname[] = MUDBAND_SERVICE_NAME;
	SERVICE_TABLE_ENTRY dispatch_table[] = {
		{ svcname, (LPSERVICE_MAIN_FUNCTION)svc_main },
		{ NULL, NULL }
	};

	if (argc > 1) {
		if (strcmp(argv[1], "-h") == 0 ||
		    strcmp(argv[1], "--help") == 0 ||
		    strcmp(argv[1], "help") == 0)
			usage();
		if (strcmp(argv[1], "install") == 0)
			return (svc_install());
		if (strcmp(argv[1], "uninstall") == 0)
			return (svc_uninstall());
	}

	if (!StartServiceCtrlDispatcher(dispatch_table)) {
		if (GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
			console_main();
			return 0;
		}
		fprintf(stderr,
		    "BANDEC_00677: StartServiceCtrlDispatcher() failed: %d",
		    GetLastError());
		return (1);
	}

	return (0);
}
