/*
 * Copyright (c) 2024 Weongyo Jeong (weongyo@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "jansson.h"
#include "odr.h"
#include "vassert.h"
#include "vhttps.h"
#include "vopt.h"
#include "vsock.h"
#include "vtc_log.h"

#include "callout.h"

#include "mudband.h"
#include "mudband_stun_client.h"

static struct vtclog *mcm_vl;
static int mcm_listen_fd = -1;
static char mcm_listen_addrstr[VSOCK_ADDRBUFSIZE];
static char mcm_listen_portstr[VSOCK_PORTBUFSIZE];
static int mcm_listen_port = -1;

static int
mcm_open_port(uint16_t port)
{
	struct sockaddr_in addr;
	int fd;
    
	fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (fd == -1)
		return (-1);    
	memset((char *)&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		ODR_close(fd);
		return (-1);
	}
	assert(fd >= 0);
	return (fd);
}

const char *
MCM_listen_portstr(void)
{

	return (mcm_listen_portstr);
}

int
MCM_listen_port(void)
{
	struct cnf *cnf;
	int listen_port = -1, r;

	if (mcm_listen_port == -1) {
		r = CNF_get(&cnf);
		if (r == 0) {
			listen_port = CNF_get_interface_listen_port(cnf->jroot);
			CNF_rel(&cnf);
		}
		if (listen_port == -1) {
			mcm_listen_fd = mcm_open_port(0);
			assert(mcm_listen_fd >= 0);
		} else {
			mcm_listen_fd = mcm_open_port(listen_port);
			if (mcm_listen_fd < 0) {
				vtc_log(mcm_vl, 1,
				    "BANDEC_00890: mcm_open_port(%d) failed."
				    " Retrying to open any port.",
				    listen_port);
				mcm_listen_fd = mcm_open_port(0);
			}
			assert(mcm_listen_fd >= 0);
		}
		assert(mcm_listen_fd >= 0);
		VSOCK_myname(mcm_listen_fd,
		    mcm_listen_addrstr, sizeof(mcm_listen_addrstr),
		    mcm_listen_portstr, sizeof(mcm_listen_portstr));
		mcm_listen_port = atoi(mcm_listen_portstr);
		vtc_log(mcm_vl, 2, "Listening on UDP %s:%s", mcm_listen_addrstr,
		    mcm_listen_portstr);
	}
	return (mcm_listen_port);
}

int
MCM_listen_fd(void)
{

	(void)MCM_listen_port();
	assert(mcm_listen_fd >= 0);
	return (mcm_listen_fd);
}

int
MCM_init(void)
{

  	mcm_vl = vtc_logopen("connmgr", NULL);
	AN(mcm_vl);
	return (0);
}
