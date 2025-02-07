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

static json_t *mpc_jroot;

static json_t *
MPC_read(void)
{
	json_t *jroot;
	json_error_t jerror;
	char filepath[ODR_BUFSIZ];

	snprintf(filepath, sizeof(filepath), "%s/mudband.conf",
	    band_confdir_root);
	if (access(filepath, F_OK) == -1) {
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

static int
MPC_write(void)
{
	FILE *fp;
	int r;
	char filepath[ODR_BUFSIZ];

	AN(mpc_jroot);
	snprintf(filepath, sizeof(filepath), "%s/mudband.conf",
	    band_confdir_root);
	fp = fopen(filepath, "w+");
	if (fp == NULL) {
		vtc_log(vl, 0, "BANDEC_XXXXX: Failed to open file %s: %s",
		    filepath, strerror(errno));
		return (-1);
	}
	r = json_dumpf(mpc_jroot, fp, 0);
	if (r == -1) {
		vtc_log(vl, 0,
		    "BANDEC_XXXXX: Failed to write JSON to file %s: %s",
		    filepath, strerror(errno));
		fclose(fp);
		return (-1);
	}
	fclose(fp);
	return (0);
}

struct mpc_traversal_dir_arg {
	int	n_enroll;
	int	b_arg_found;
	char	b_arg_uuidstr[64];
};

static int
MPC_get_default_band_uuid_traversal_dir_callback(struct vtclog *mpc_vl,
    const char *name, void *orig_arg)
{
	struct mpc_traversal_dir_arg *arg =
		(struct mpc_traversal_dir_arg *)orig_arg;
	int namelen;
	char band_uuidstr[64];

	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		return (0);
	namelen = (int)strlen(name);
	if (namelen < sizeof("band_0b0a3721-7dc0-4391-969d-b3b0d1e00925.json") - 1)
		return (0);
	if (strncmp(name, "band_", sizeof("band_") - 1) != 0)
		return (0);
	if (strcmp(name + namelen - 5, ".json") != 0)
		return (0);
	snprintf(band_uuidstr, sizeof(band_uuidstr),
	    "%.*s", namelen - 5 - ((int)sizeof("band_") - 1),
	    name + sizeof("band_") - 1);
	snprintf(arg->b_arg_uuidstr, sizeof(arg->b_arg_uuidstr), "%s",
	    band_uuidstr);
	vtc_log(mpc_vl, 2, "Found enrollment for the default band UUID: %s/%s",
	    band_confdir_enroll, name);
	arg->n_enroll++;
	return (0);
}

const char *
MPC_get_default_band_uuid(void)
{
	json_t *default_band_uuid;

	AN(mpc_jroot);
	default_band_uuid = json_object_get(mpc_jroot, "default_band_uuid");
	if (default_band_uuid == NULL) {
		struct mpc_traversal_dir_arg dir_arg = { 0, };
		int r;
        
		r = ODR_traversal_dir(vl, band_confdir_enroll,
		    MPC_get_default_band_uuid_traversal_dir_callback,
		    (void *)&dir_arg);
		if (r != 0) {
			vtc_log(vl, 0,
			    "BANDEC_00122: ODR_traversal_dir() failed");
			return (NULL);
		}
		if (dir_arg.n_enroll == 0) {
			vtc_log(vl, 0,
			    "BANDEC_00123: No enrollments found.");
			return (NULL);
		}
		assert(dir_arg.n_enroll > 0);
		MPC_set_default_band_uuid(dir_arg.b_arg_uuidstr);
		default_band_uuid = json_object_get(mpc_jroot,
		    "default_band_uuid");
		AN(default_band_uuid);
	}
	return (json_string_value(default_band_uuid));
}

void
MPC_set_default_band_uuid(const char *band_uuid)
{
	json_t *default_band_uuid;
    
	AN(mpc_jroot);
	default_band_uuid = json_object_get(mpc_jroot, "default_band_uuid");
	if (default_band_uuid != NULL) {
		json_object_del(mpc_jroot, "default_band_uuid");
	}
	json_object_set_new(mpc_jroot, "default_band_uuid",
	    json_string(band_uuid));
	MPC_write();
}

void
MPC_init(void)
{
    
	mpc_jroot = MPC_read();
	if (mpc_jroot == NULL) {
		mpc_jroot = json_object();
		AN(mpc_jroot);
		(void)MPC_write();
	}
	AN(mpc_jroot);
}
