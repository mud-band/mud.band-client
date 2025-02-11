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

#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "miniobj.h"
#include "odr.h"
#include "odr_pthread.h"
#include "vassert.h"
#include "vtc_log.h"

#include "mudband_service.h"

struct cmdctl {
	pid_t		pid;
	pthread_t	tp;
	int		fds[6];
	int		vl_log;
	int		wait;
	const char	*prefix;
	struct vtclog	*vl;
};
static struct cmdctl	*cmdctl;

static void	cmd_finish(struct cmdctl *v);

static int
CMD_nonblocking(int sock)
{
        int i, j;

        i = 1;
        j = ioctl(sock, FIONBIO, &i);
	assert(j == 0);
        return (j);
}

static void *
CMD_thread(void *priv)
{
	struct cmdctl *v;
	struct pollfd *fds, fd;
	char buf[BUFSIZ], *p, *q;
	int i, offset = 0;

	v = (struct cmdctl *)priv;
	AN(v);
	CMD_nonblocking(v->fds[0]);
	while (1) {
		fds = &fd;
		memset(fds, 0, sizeof *fds);
		fds->fd = v->fds[0];
		fds->events = POLLIN;
		i = poll(fds, 1, 1000);
		if (i == 0)
			continue;
		if (fds->revents & (POLLERR|POLLHUP))
			break;
		assert((int)sizeof(buf) - 1 - offset > 0);
		i = read(v->fds[0], buf + offset, sizeof(buf) - 1 - offset);
		if (i <= 0)
			break;
		assert(i + offset >= 0);
		assert(i + offset < sizeof(buf));
		buf[i + offset] = '\0';
		p = buf;
		while (p < &buf[i + offset]) {
			q = strchr(p, '\n');
			if (q == NULL) {
				assert(&buf[i + offset] >= p);
				offset = &buf[i + offset] - p;
				assert(offset < sizeof(buf));
				if (p != buf && offset > 0) {
					memmove(buf, p, offset);
				}
				break;
			}
			*q = '\0';
			if (v->vl_log)
				vtc_log(v->vl, 3, "%s%s", v->prefix, p);
			else
				printf("%s%s\n", v->prefix, p);
			p = q + 1;
			offset = 0;
		}
	}
	cmd_finish(v);
	return (NULL);
}

static void
cmd_finish(struct cmdctl *v)
{
	int l, r, status;
	char msg[1024];

	r = wait4(v->pid, &status, 0, NULL);
	v->pid = 0;
	l = snprintf(msg, sizeof(msg), "R %d Status: %04x", r, status);
	if (!WIFEXITED(status) ||
	    WEXITSTATUS(status) != 0) {
		l += snprintf(msg + l, sizeof(msg) - l,
		    " Bad exit code: %04x sig %x exit %x",
		    status, WTERMSIG(status), WEXITSTATUS(status));
	}
	l += snprintf(msg + l, sizeof(msg) - l, "\n");
	if (v->vl_log)
		vtc_log(v->vl, 3, "%s%s", v->prefix, msg);
	else
		printf("%s", msg);
	/* May already have been closed */
	(void)close(v->fds[1]);
	(void)close(v->fds[0]);
	free(v);
}

static void
cmd_pdeathsig(void)
{
	int error;

	error = prctl(PR_SET_PDEATHSIG, SIGHUP);
	if (error != 0)
		printf("[ERROR] prctl(2) failed: %d %s.", errno,
		    strerror(errno));
}

static char **
cmd_argv_parse(const char *cmd, int *argc)
{
	char **argv;
	char *token, *str, *tofree;
	int count;

	argv = NULL;
	count = 0;
	tofree = str = strdup(cmd);
	if (str == NULL)
		return (NULL);
	token = str;
	while (*token != '\0') {
		while (*token != '\0' && isspace(*token))
			token++;
		if (*token != '\0') {
			count++;
			while (*token != '\0' && !isspace(*token))
				token++;
		}
	}
	argv = calloc(count + 1, sizeof(char *));
	if (argv == NULL) {
		free(tofree);
		return (NULL);
	}
	str = tofree;
	count = 0;
	while ((token = strsep(&str, " \t\n")) != NULL) {
		if (*token != '\0') {
			argv[count] = strdup(token);
			count++;
		}
	}
	argv[count] = NULL;
	*argc = count;
	free(tofree);
	return (argv);
}

static void
cmd_argv_free(char **argv)
{
	char **p;

	if (argv == NULL)
		return;
	for (p = argv; *p != NULL; p++)
		free(*p);
	free(argv);
}

static void
cmd_runchild(void)
{
	struct pollfd *fds, fd;
	int error, i, offset = 0;
	char buf[BUFSIZ], *p, *q;

	error = prctl(PR_SET_PDEATHSIG, SIGHUP);
	if (error != 0)
		vtc_log(cmdctl->vl, 0, "prctl(2) failed: %d %s.", errno,
		    strerror(errno));

	assert(dup2(cmdctl->fds[0], 0) == 0);
	assert(dup2(cmdctl->fds[3], 1) == 1);
	assert(dup2(1, 2) == 2);
	assert(dup2(cmdctl->fds[4], 3) == 3);
	AZ(close(cmdctl->fds[0]));
	AZ(close(cmdctl->fds[1]));
	AZ(close(cmdctl->fds[2]));
	AZ(close(cmdctl->fds[3]));
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	CMD_nonblocking(cmdctl->fds[4]);
	while (1) {
		fds = &fd;
		memset(fds, 0, sizeof *fds);
		fds->fd = cmdctl->fds[4];
		fds->events = POLLIN;
		i = poll(fds, 1, 1000);
		if (i == 0)
			continue;
		if (fds->revents & (POLLERR|POLLHUP))
			break;
		i = read(cmdctl->fds[4], buf + offset, sizeof buf - 1 - offset);
		if (i <= 0)
			break;
		buf[i] = '\0';
		p = buf;
		while (1) {
			q = strchr(p, '\n');
			if (q == NULL) {
				offset = &buf[i] - p;
				memmove(buf, p, offset);
				break;
			}
			offset = 0;
			*q = '\0';
			{
				struct cmdctl *v;
				char **argv;
				int argc;

				v = (struct cmdctl *)calloc(sizeof(*v), 1);
				AN(v);
				AZ(pipe(&v->fds[0]));
				AZ(pipe(&v->fds[2]));
				v->prefix = "";
				v->vl_log = 0;
				v->wait = (p[0] == '!') ? 1 : 0;

				argv = cmd_argv_parse(p + 1, &argc);
				if (!argv || argc == 0) {
					vtc_log(cmdctl->vl, 0,
					    "Failed to parse command: %s",
					    p + 1);
					cmd_argv_free(argv);
					free(v);
					continue;
				}

				v->pid = fork();
				assert(v->pid >= 0);
				if (v->pid == 0) {
					assert(dup2(v->fds[0], 0) == 0);
					assert(dup2(v->fds[3], 1) == 1);
					assert(dup2(1, 2) == 2);
					AZ(close(v->fds[0]));
					AZ(close(v->fds[1]));
					AZ(close(v->fds[2]));
					AZ(close(v->fds[3]));
					for (i = 3; i < getdtablesize(); i++)
						(void)close(i);
					cmd_pdeathsig();
					printf("Executing %s (wait %d)\n",
					    p + 1, v->wait);
					i = execvp(argv[0], argv);
					assert(i == -1);
					printf("Failed to execute %s: %s\n",
					    argv[0], strerror(errno));
					exit(1);
				}
				cmd_argv_free(argv);
				close(v->fds[0]);
				close(v->fds[3]);
				v->fds[0] = v->fds[2];
				v->fds[2] = v->fds[3] = -1;
				AZ(pthread_create(&v->tp, NULL, CMD_thread, v));
				if (v->wait)
					cmd_finish(v);
			}
			p = q + 1;
		}
	}
	vtc_log(cmdctl->vl, 0, "Command controller exited.\n");
	exit(1);
}

int
CMD_execute(int wait, const char *fmt, ...)
{
	va_list ap;
	ssize_t l;
	int buflen;
	char buf[BUFSIZ];

	if (wait)
		buf[0] = '!';
	else
		buf[0] = ';';
	va_start(ap, fmt);
	buflen = vsnprintf(buf + 1, sizeof(buf) - 1, fmt, ap);
	va_end(ap);
	assert(buflen < sizeof(buf));

	vtc_log(cmdctl->vl, 4, "Queueing command \"%.*s\"", buflen, buf); 

	l = write(cmdctl->fds[5], buf, buflen + 1);
	if (l == -1) {
		vtc_log(cmdctl->vl, 0,
		    "write(2) error for command \"%s\": %d %s\n", buf,
		    errno, strerror(errno));
		return (-1);
	}
	if (l != buflen + 1) {
		vtc_log(cmdctl->vl, 0,
		    "write(2) error for command \"%s\":"
		    " short write\n", buf);
		return (-1);
	}
	return (0);
}

void
CMD_init(void)
{

	cmdctl = (struct cmdctl *)calloc(1, sizeof(*cmdctl));
	AN(cmdctl);
	cmdctl->prefix = "Child said: ";
	cmdctl->vl = vtc_logopen("cmd", NULL);
	assert(cmdctl->vl != NULL);
	cmdctl->vl_log = 1;
	AZ(pipe(&cmdctl->fds[0]));
	AZ(pipe(&cmdctl->fds[2]));
	AZ(pipe(&cmdctl->fds[4]));
	cmdctl->pid = fork();
	assert(cmdctl->pid >= 0);
	if (cmdctl->pid == 0)
		cmd_runchild();
	AZ(close(cmdctl->fds[0]));
	AZ(close(cmdctl->fds[3]));
	AZ(close(cmdctl->fds[4]));
	cmdctl->fds[0] = cmdctl->fds[2];
	cmdctl->fds[2] = cmdctl->fds[3] = -1;
	AZ(pthread_create(&cmdctl->tp, NULL, CMD_thread, cmdctl));
}
