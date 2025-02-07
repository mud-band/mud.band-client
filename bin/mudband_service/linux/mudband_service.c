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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "linux/vpf.h"
#include "jansson.h"
#include "odr.h"
#include "vassert.h"
#include "vtc_log.h"

#include "mudband_service.h"

static struct vtclog *vl;
static const char *u_arg;

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
	/* TODO: Implement watchdog */
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

static void
PID_init(const char *P_arg)
{
	struct vpf_fh *pfh;

	pfh = VPF_Open(P_arg, 0644, NULL);
	if (pfh == NULL) {
		if (errno == EAGAIN) {
			vtc_log(vl, VTCLOG_LEVEL_WARNING,
			    "BANDEC_XXXXX: muddog_srv is already running."
			    "  Exit.");
			exit(1);
		}
		vtc_log(vl, VTCLOG_LEVEL_WARNING,
		    "BANDEC_XXXXX: VPF_Open() failed: %d %s", errno,
	    	    strerror(errno));
		exit(0);
	}
	if (VPF_Write(pfh)) {
		vtc_log(vl, VTCLOG_LEVEL_ERROR,
		    "BANDEC_XXXXX: Could not write PID file.");
		exit(1);
	}
}

static void
main_loop(int fd)
{
	struct sockaddr_un from;
	socklen_t fromlen;
	ssize_t buflen, outlen;
	char buf[ODR_BUFSIZ], out[ODR_BUFSIZ];
	int rv;

	while (1) {
		struct timeval tv;
		json_t *root = NULL, *cmd;
		json_error_t error;
		fd_set set;
		int cfd;

		FD_ZERO(&set);
		FD_SET(fd, &set);
		bzero(&tv, sizeof(tv));
		tv.tv_sec = 3;
		rv = select(fd + 1, &set, NULL, NULL, &tv);
		if (rv == -1) {
			vtc_log(vl, 0, "BANDEC_XXXXX: select(2) error: %d %s",
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
			vtc_log(vl, 0, "BANDEC_XXXXX: accept(2) error: %d %s",
				errno, strerror(errno));
			sleep(1);
			continue;
		}
		buflen = read(cfd, buf, sizeof(buf));
		if (buflen < 0) {
			vtc_log(vl, 0, "BANDEC_XXXXX: read(2) failed: %d %s",
				errno, strerror(errno));
			sleep(1);
			goto next;
		}
		if (buflen == 0) {
			vtc_log(vl, 0, "BANDEC_XXXXX: Too short message.");
			sleep(1);
			goto next;
		}
		assert(buflen > 0);
		buf[buflen] = '\0';
		root = json_loads(buf, 0, &error);
		if (root == NULL) {
			vtc_log(vl, 0, "BANDEC_XXXXX: json_loads() failed: %s",
				error.text);
			goto next;
		}
		if (!json_is_object(root)) {
			vtc_log(vl, 0, "BANDEC_XXXXX: invalid message");
			goto next;
		}
		cmd = json_object_get(root, "cmd");
		if (!json_is_string(cmd)) {
			vtc_log(vl, 0, "BANDEC_XXXXX: invalid message");
			goto next;
		}
		if (strcmp(json_string_value(cmd), "ping") == 0) {
			outlen = cmd_ping(out, sizeof(out));
		} else {
			vtc_log(vl, 0, "BANDEC_XXXXX: unknown command: %s",
				json_string_value(cmd));
			goto next;
		}
		assert(outlen > 0);
		rv = write(cfd, out, outlen);
		if (rv < 0) {
			vtc_log(vl, 0, "BANDEC_XXXXX: sendto(2) failed: %d %s",
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

int
main(int argc, char *argv[])
{
	struct sockaddr_un addr;
	struct passwd *pw;
	uid_t owner = -1;
	int fd, o, rv;
	const char *P_arg = "/var/run/mudband_service.pid";
	const char *S_arg = "/var/run/mudband_service.sock";

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

	ODR_libinit();
	vl = vtc_logopen("srv", NULL);
	assert(vl != NULL);
	CMD_ctl();
	PID_init(P_arg);

	if ((fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		vtc_log(vl, 0, "BANDEC_XXXXX: socket(2) failed: %d %s", errno,
		    strerror(errno));
		return (1);
	}
	unlink(S_arg);
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, S_arg);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		vtc_log(vl, 0, "BANDEC_XXXXX: bind(2) failed: %d %s", errno,
		    strerror(errno));
		return (1);
	}
	if (listen(fd, 32) < 0) {
		vtc_log(vl, 0, "BANDEC_XXXXX: listen(2) failed: %d %s", errno,
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
