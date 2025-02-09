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
#include "vhttps.h"
#include "vtc_log.h"

#include "wireguard.h"

#include "mudband_service.h"

static json_t *
cnf_read(const char *filename)
{
	json_t *jroot;
	json_error_t jerror;
	char filepath[ODR_BUFSIZ];

	ODR_snprintf(filepath, sizeof(filepath), "%s/%s",
	    band_confdir_enroll, filename);
	if (ODR_access(filepath, ODR_ACCESS_F_OK) == -1) {
		vtc_log(vl, 0, "BANDEC_XXXXX: File not found: %s",
		    filepath);
		return (NULL);
	}
	jroot = json_load_file(filepath, 0, &jerror);
	if (jroot == NULL) {
		vtc_log(vl, 1,
		    "BANDEC_XXXXX: error while reading JSON format:"
		    " on line %d: %s", jerror.line, jerror.text);
		return (NULL);
	}
	return (jroot);
}

json_t *
CNF_get_active_conf(void)
{
	const char *default_band_uuid;
	json_t *active_conf;
	char filepath[ODR_BUFSIZ];

	default_band_uuid = MPC_get_default_band_uuid();
	AN(default_band_uuid);
	ODR_snprintf(filepath, sizeof(filepath), "conf_%s.json",
	    default_band_uuid);
	active_conf = cnf_read(filepath);
	if (active_conf == NULL) {
		return (NULL);
	}
	return (active_conf);
}
