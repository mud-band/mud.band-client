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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "linux/vpf.h"
#include "jansson.h"
#include "odr.h"
#include "vassert.h"
#include "vhttps.h"
#include "vtc_log.h"

#include "crypto.h"
#include "wireguard.h"

#include "mudband_service.h"

struct tunnel_status {
	int is_running;
};
static struct tunnel_status band_tunnel_status;

char *band_confdir_enroll;
char *band_confdir_root;
struct vtclog *vl;

static void	check_tunnel_status(void);

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

void
VAS_Fail(const char *func, const char *file, int line, const char *cond,
	    int xxx)
{

	fprintf(stdout, "Critical! assert fail: %s %s:%d %s %d\n", func, file,
	    line, cond, xxx);
	abort();
}

static void
watchdog(void)
{

	check_tunnel_status();
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
cmd_get_band_name(char *out, size_t outmax)
{
	json_t *root;
	ssize_t outlen = 0;
	int r;
	char band_name[ODR_BUFSIZ];
	char *p;

	root = json_object();
	assert(root != NULL);

	r = MBE_get_band_name(band_name, sizeof(band_name));
	if (r != 0) {
		json_object_set_new(root, "status", json_integer(500));
		json_object_set_new(root, "msg",
		    json_string("MBE_get_band_name() failed"));
	} else {
		json_object_set_new(root, "status", json_integer(200));
		json_object_set_new(root, "band_name",
		    json_string(band_name));
	}
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
		    json_string("MBE_get_enrollment_count() failed"));
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

static int
check_process_running(const char *pidfile)
{
	FILE *fp;
	char buf[32];
	pid_t pid;
	
	fp = fopen(pidfile, "r");
	if (fp == NULL) {
		if (errno == ENOENT)
			return (0);
		vtc_log(vl, VTCLOG_LEVEL_ERROR, 
		    "BANDEC_00549: Failed to open PID file: %s",
		    strerror(errno));
		return (-1);
	}
	if (fgets(buf, sizeof(buf), fp) == NULL) {
		fclose(fp);
		return (-1);
	}
	fclose(fp);
	pid = atoi(buf);
	if (pid <= 0)
		return (-1);
	if (kill(pid, 0) == 0)
		return (1);
	if (errno == ESRCH)
		return (0);
	return (-1);
}

static void
PID_init(const char *P_arg)
{
	struct vpf_fh *pfh;

	pfh = VPF_Open(P_arg, 0644, NULL);
	if (pfh == NULL) {
		if (errno == EAGAIN) {
			vtc_log(vl, VTCLOG_LEVEL_WARNING,
			    "BANDEC_00550: muddog_srv is already running."
			    "  Exit.");
			exit(1);
		}
		vtc_log(vl, VTCLOG_LEVEL_WARNING,
		    "BANDEC_00551: VPF_Open() failed: %d %s", errno,
	    	    strerror(errno));
		exit(0);
	}
	if (VPF_Write(pfh)) {
		vtc_log(vl, VTCLOG_LEVEL_ERROR,
		    "BANDEC_00552: Could not write PID file.");
		exit(1);
	}
}

static ssize_t
cmd_tunnel_connect(char *out, size_t outmax)
{
	json_t *root;
	ssize_t outlen = 0;
	char *p;
	int rv;

	root = json_object();
	assert(root != NULL);

	if (band_tunnel_status.is_running) {
		json_object_set_new(root, "status", json_integer(400));
		json_object_set_new(root, "msg", 
			json_string("Tunnel is already running"));
	} else {
		rv = CMD_Execute(0, "/opt/mudband/0.0.9/bin/mudband");
		if (rv == 0) {
			band_tunnel_status.is_running = 1;
			json_object_set_new(root, "status", json_integer(200));
			json_object_set_new(root, "msg", 
				json_string("Tunnel started successfully"));
		} else {
			json_object_set_new(root, "status", json_integer(500));
			json_object_set_new(root, "msg", 
				json_string("Failed to start tunnel"));
		}
	}
	p = json_dumps(root, 0);
	assert(p != NULL);
	outlen = snprintf(out, outmax, "%s", p);
	free(p);
	json_decref(root);
	return (outlen);
}

static int
stop_tunnel_process(const char *pidfile)
{
	FILE *fp;
	char buf[32];
	pid_t pid;
	int rv = -1;
	int retry = 5;

	fp = fopen(pidfile, "r");
	if (fp == NULL) {
		return (-1);
	}

	if (fgets(buf, sizeof(buf), fp) != NULL) {
		pid = atoi(buf);
		if (pid > 0) {
			rv = kill(pid, SIGTERM);
			if (rv == 0) {
				while (retry-- > 0) {
					if (kill(pid, 0) < 0 && errno == ESRCH) {
						break;
					}
					sleep(1);
				}
				if (retry < 0) {
					kill(pid, SIGKILL);
				}
			}
		}
	}
	fclose(fp);
	unlink(pidfile);
	return (rv);
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
		json_object_set_new(root, "status", json_integer(400));
		json_object_set_new(root, "msg", 
			json_string("Tunnel is not running"));
	} else {
		rv = stop_tunnel_process("/var/run/mudband.pid");
		if (rv == -1 && errno == ENOENT) {
			band_tunnel_status.is_running = 0;
			json_object_set_new(root, "status", json_integer(200));
			json_object_set_new(root, "msg", 
				json_string("Tunnel status updated"));
		} else if (rv == 0) {
			band_tunnel_status.is_running = 0;
			json_object_set_new(root, "status", json_integer(200));
			json_object_set_new(root, "msg", 
				json_string("Tunnel stopped successfully"));
		} else {
			json_object_set_new(root, "status", json_integer(500));
			json_object_set_new(root, "msg", 
				json_string("Failed to stop tunnel"));
		}
	}

	p = json_dumps(root, 0);
	assert(p != NULL);
	outlen = snprintf(out, outmax, "%s", p);
	free(p);
	json_decref(root);
	return (outlen);
}

static void
main_loop(int fd)
{
	struct sockaddr_un from;
	struct timeval tv;
	json_t *root, *cmd;
	json_error_t error;
	fd_set set;
	socklen_t fromlen;
	ssize_t buflen, outlen;
	const char *cmdval;
	char buf[ODR_BUFSIZ], out[ODR_BUFSIZ];
	int rv, cfd;

	for (;;) {
		FD_ZERO(&set);
		FD_SET(fd, &set);
		bzero(&tv, sizeof(tv));
		tv.tv_sec = 3;

		rv = select(fd + 1, &set, NULL, NULL, &tv);
		if (rv == -1) {
			vtc_log(vl, 0, "BANDEC_00553: select(2) error: %d %s",
			    errno, strerror(errno));
			sleep(1);
			continue;
		}
		if (rv == 0) {
			watchdog();
			continue;
		}
		if (!FD_ISSET(fd, &set))
			continue;

		fromlen = sizeof(from);
		cfd = accept(fd, (struct sockaddr *)&from, &fromlen);
		if (cfd == -1) {
			vtc_log(vl, 0, "BANDEC_00554: accept(2) error: %d %s",
			    errno, strerror(errno));
			sleep(1);
			continue;
		}

		buflen = read(cfd, buf, sizeof(buf));
		if (buflen < 0) {
			vtc_log(vl, 0, "BANDEC_00555: read(2) failed: %d %s",
			    errno, strerror(errno));
			sleep(1);
			goto next;
		}
		if (buflen == 0) {
			vtc_log(vl, 0, "BANDEC_00556: Too short message.");
			sleep(1);
			goto next;
		}
		assert(buflen > 0);
		buf[buflen] = '\0';
		fprintf(stdout, "REQ: %s\n", buf);

		root = json_loads(buf, 0, &error);
		if (root == NULL) {
			vtc_log(vl, 0, "BANDEC_00557: json_loads() failed: %s",
			    error.text);
			goto next;
		}
		if (!json_is_object(root)) {
			vtc_log(vl, 0, "BANDEC_00558: Invalid message");
			goto next;
		}

		cmd = json_object_get(root, "cmd");
		if (!json_is_string(cmd)) {
			vtc_log(vl, 0, "BANDEC_00559: Invalid message");
			goto next;
		}

		cmdval = json_string_value(cmd);
		if (strcmp(cmdval, "enroll") == 0)
			outlen = cmd_enroll(out, sizeof(out), root);
		else if (strcmp(cmdval, "get_band_name") == 0)
			outlen = cmd_get_band_name(out, sizeof(out));
		else if (strcmp(cmdval, "get_enrollment_count") == 0)
			outlen = cmd_get_enrollment_count(out, sizeof(out));
		else if (strcmp(cmdval, "ping") == 0)
			outlen = cmd_ping(out, sizeof(out));
		else if (strcmp(cmdval, "tunnel_get_status") == 0)
			outlen = cmd_tunnel_get_status(out, sizeof(out));
		else if (strcmp(cmdval, "tunnel_connect") == 0)
			outlen = cmd_tunnel_connect(out, sizeof(out));
		else if (strcmp(cmdval, "tunnel_disconnect") == 0)
			outlen = cmd_tunnel_disconnect(out, sizeof(out));
		else {
			vtc_log(vl, 0, "BANDEC_00560: Unknown command: %s",
			    json_string_value(cmd));
			goto next;
		}

		if (outlen < 0) {
			vtc_log(vl, 0, "BANDEC_00561: %s command failed",
			    cmdval);
			goto next;
		}
		assert(outlen > 0);
		out[outlen] = '\0';
		fprintf(stdout, "RESP: %s\n", out);

		rv = write(cfd, out, outlen);
		if (rv < 0) {
			vtc_log(vl, 0, "BANDEC_00562: sendto(2) failed: %d %s",
			    errno, strerror(errno));
			goto next;
		}

next:
		if (root != NULL)
			json_decref(root);
		close(cfd);
	}
}

static void
usage(void)
{
#define FMT "    %-20s # %s\n"
#define FMT_INDENT "                           "
	fprintf(stdout, "Usage: mudband_service [-h] [-P pidfile]"
	    " [-S sockfile] [-u user]\n");
	fprintf(stdout, FMT, "-h", "Show this help message");
	fprintf(stdout, FMT, "-P pidfile", "PID file path");
	fprintf(stdout, FMT_INDENT "(default: /var/run/mudband_service.pid)\n");
	fprintf(stdout, FMT, "-S sockfile", "Socket file path"); 
	fprintf(stdout, FMT_INDENT
	    "(default: /var/run/mudband_service.sock)\n");
	fprintf(stdout, FMT, "-u user",
	    "Make the socket file owned by the specified user");
	exit(1);
}

static void
check_tunnel_status(void)
{
	int is_running;
	
	is_running = check_process_running("/var/run/mudband.pid");
	if (is_running < 0) {
		vtc_log(vl, VTCLOG_LEVEL_ERROR,
			"BANDEC_00563: Failed to check running process");
		return;
	}
	if (is_running > 0) {
		band_tunnel_status.is_running = 1;
	} else {
		band_tunnel_status.is_running = 0;
	}
}

static void
init(const char *pidpath)
{
	int rv;
	const char *cdir;
	char edir[ODR_BUFSIZ];

	ODR_libinit();
	vtc_loginit();
	vl = vtc_logopen("srv", NULL);
	assert(vl != NULL);
	rv = ODR_corefile_init();
	if (rv != 0) {
		vtc_log(vl, 1,
		    "BANDEC_00564: Failed to initialize the corefile"
		    " handler: %d %s", ODR_errno(), ODR_strerror(ODR_errno()));
	}
	CMD_ctl();
	PID_init(pidpath);
	VHTTPS_init();

	cdir = ODR_confdir();
	ODR_mkdir_recursive(cdir);
	band_confdir_root = ODR_strdup(cdir);
	AN(band_confdir_root);
	ODR_snprintf(edir, sizeof(edir), "%s/enroll", band_confdir_root);
	ODR_mkdir_recursive(edir);
	band_confdir_enroll = ODR_strdup(edir);
	AN(band_confdir_enroll);

	MPC_init();
	check_tunnel_status();
}

int
main(int argc, char *argv[])
{
	struct sockaddr_un addr;
	struct passwd *pw;
	uid_t owner = -1;
	int fd, o, rv;
	const char *P_arg = "/var/run/mudband_service.pid";
	const char *S_arg = "/var/run/mudband_service.sock";
	const char *u_arg = NULL;

	while ((o = getopt(argc, argv, "hP:S:u:")) != -1)
		switch (o) {
		case 'h':
			usage();
			break;
		case 'P':
			P_arg = optarg;
			break;
		case 'S':
			S_arg = optarg;
			break;
		case 'u':
			u_arg = optarg;
			pw = getpwnam(u_arg);
			assert(pw != NULL);
			owner = pw->pw_uid;
			break;
		default:
			fprintf(stderr, "[ERROR] Unknown option '%c'\n", o);
			exit(1);
		}
	argc -= optind;
	argv += optind;

	init(P_arg);

	if ((fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		vtc_log(vl, 0, "BANDEC_00565: socket(2) failed: %d %s", errno,
		    strerror(errno));
		return (1);
	}
	unlink(S_arg);
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, S_arg);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		vtc_log(vl, 0, "BANDEC_00566: bind(2) failed: %d %s", errno,
		    strerror(errno));
		return (1);
	}
	if (listen(fd, 32) < 0) {
		vtc_log(vl, 0, "BANDEC_00567: listen(2) failed: %d %s", errno,
		    strerror(errno));
		return (1);
	}
	rv = chmod(S_arg, 0600);
	assert(rv == 0);
	if (owner != -1) {
		rv = chown(S_arg, owner, -1);
		assert(rv == 0);
	}
	main_loop(fd);
	return (0);
}
