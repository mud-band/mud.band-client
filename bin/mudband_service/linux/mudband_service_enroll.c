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
#include "vhttps.h"
#include "vtc_log.h"

#include "wireguard.h"

#include "mudband_service.h"

struct mbe_traversal_dir_arg {
	int	n_enroll;
	int	b_arg_found;
	char	b_arg_uuidstr[64];
	json_t *jroot;
};

static void
mbe_file_delete(const char *filepath)
{
	int r;

	r = ODR_unlink(filepath);
	assert(r == 0 || (r == -1 && errno == ENOENT));
}

static int
mbe_file_write(const char *filepath, json_t *obj)
{
	FILE *fp;
	int r;

	fp = fopen(filepath, "w+");
	if (fp == NULL) {
		vtc_log(vl, 0, "BANDEC_00574: Failed to open file %s: %s",
		    filepath, strerror(errno));
		return (-1);
	}
	r = json_dumpf(obj, fp, 0);
	if (r == -1) {
		vtc_log(vl, 0,
		    "BANDEC_00575: Failed to write JSON to file %s: %s",
		    filepath, strerror(errno));
		fclose(fp);
		return (-1);
	}
	fclose(fp);
	return (0);
}

ssize_t
MBE_enroll(char *out, size_t outmax, const char *token, const char *name, const char *secret)
{
	struct vhttps_req req;
	json_t *jroot, *jstatus, *jband, *juuid, *jband_name, *jopt_public;
	json_t *jmsg, *jsso_url;
	json_error_t jerror;
	ssize_t outlen;
	size_t wg_pubkeystrlen, wg_privkeystrlen;
	int r, req_bodylen, status;
	char req_body[ODR_BUFSIZ];
	char filepath[ODR_BUFSIZ];
	uint8_t wg_privkey[WIREGUARD_PRIVATE_KEY_LEN];
	uint8_t wg_pubkey[WIREGUARD_PUBLIC_KEY_LEN];
	char wg_pubkeystr[WIREGUARD_PUBLIC_KEY_LEN * 2 + 1 /* XXX */];
	char wg_privkeystr[WIREGUARD_PRIVATE_KEY_LEN * 2 + 1 /* XXX */];
	bool success;

	vtc_log(vl, 2, "Enrolling with token: %s (name %s)", token, name);
	/* generate wireguard key pair */
	wireguard_generate_private_key(wg_privkey);
	wireguard_generate_public_key(wg_pubkey, wg_privkey);
	wg_pubkeystrlen = sizeof(wg_pubkeystr);
	success = wireguard_base64_encode(wg_pubkey, sizeof(wg_pubkey),
	    wg_pubkeystr, &wg_pubkeystrlen);
	if (!success) {
		vtc_log(vl, 0,
		    "BANDEC_00576: wireguard_base64_encode() failed.");
		return (-1);
	}
	wg_privkeystrlen = sizeof(wg_privkeystr);
	success = wireguard_base64_encode(wg_privkey, sizeof(wg_privkey),
	    wg_privkeystr, &wg_privkeystrlen);
	if (!success) {
		vtc_log(vl, 0,
		    "BANDEC_00577: wireguard_base64_encode() failed.");
		return (-1);
	}
	wg_pubkeystr[wg_pubkeystrlen] = '\0';
	req_bodylen = ODR_snprintf(req_body, sizeof(req_body),
	    "{"
	    "  \"token\": \"%s\","
	    "  \"name\": \"%s\","
	    "  \"secret\": \"%s\","
	    "  \"wireguard_pubkey\": \"%s\""
	    "}", token, name, secret, wg_pubkeystr);
	ODR_bzero(&req, sizeof(req));
	req.vl = vl;
	req.server = "www.mud.band:443";
	req.domain = "www.mud.band";
	req.url = "/api/band/enroll";
	req.hdrs = "Content-Type: application/json\r\n"
	    "Host: www.mud.band\r\n";
	req.body = req_body;
	req.bodylen = req_bodylen;

	outlen = (ssize_t)outmax;
	r = VHTTPS_post(&req, out, (size_t *)&outlen);
	if (r == -1) {
		vtc_log(vl, 0, "BANDEC_00578: VHTTPS_post() failed.");
		return (-1);
	}
	assert(outlen >= 0);
	out[outlen] = '\0';

	jroot = json_loads(out, 0, &jerror);
	if (jroot == NULL) {
		vtc_log(vl, 1,
		    "BANDEC_00579: error while parsing JSON format:"
		    " on line %d: %s", jerror.line, jerror.text);
		vtc_log(vl, 1,
		    "BANDEC_00580: response body: %s", out);
		return (-1);
	}
	jstatus = json_object_get(jroot, "status");
	AN(jstatus);
	assert(json_is_integer(jstatus));
	status = json_integer_value(jstatus);
	switch (status) {
	case 200:
		break;
	case 301:
		jsso_url = json_object_get(jroot, "sso_url");
		AN(jsso_url);
		assert(json_is_string(jsso_url));
		assert(json_string_length(jsso_url) > 0);
		vtc_log(vl, 2,
		    "MFA (multi-factor authentication) is enabled to enroll."
		    " Please visit the following URL: %s",
		    json_string_value(jsso_url));
		json_decref(jroot);
		return (outlen);
	default:
		jmsg = json_object_get(jroot, "msg");
		AN(jmsg);
		assert(json_is_string(jmsg));
		vtc_log(vl, 1,
		    "BANDEC_00581: Failed to enroll. (reason %s)",
		    json_string_value(jmsg));
		json_decref(jroot);
		return (outlen);
	}
	jband = json_object_get(jroot, "band");
	AN(jband);
	assert(json_is_object(jband));
	juuid = json_object_get(jband, "uuid");
	AN(juuid);
	assert(json_is_string(juuid));
	jband_name = json_object_get(jband, "name");
	AN(jband_name);
	assert(json_is_string(jband_name));
	jopt_public = json_object_get(jband, "opt_public");
	AN(jopt_public);
	assert(json_is_integer(jopt_public));
	json_object_set_new(jband, "wireguard_privkey",
	    json_string(wg_privkeystr));
	ODR_snprintf(filepath, sizeof(filepath), "%s/band_%s.json",
	    band_confdir_enroll, json_string_value(juuid));
	r = mbe_file_write(filepath, jband);
	assert(r == 0);
	MPC_set_default_band_uuid(json_string_value(juuid));
	vtc_log(vl, 2, "Enrolled in the band: %s (uuid %s)",
	    json_string_value(jband_name), json_string_value(juuid));
	/* delete the previous cached config. */
	ODR_snprintf(filepath, sizeof(filepath), "%s/conf_%s.json",
	    band_confdir_enroll, json_string_value(juuid));
	mbe_file_delete(filepath);
	json_decref(jroot);
	return (outlen);
}

static int
mbe_traversal_dir_callback(struct vtclog *mbe_vl, const char *name,
    void *orig_arg)
{
	struct mbe_traversal_dir_arg *arg =
	    (struct mbe_traversal_dir_arg *)orig_arg;
	int namelen;

	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		return (0);
	namelen = strlen(name);
	if (namelen < sizeof("band_0b0a3721-7dc0-4391-969d-b3b0d1e00925.json") - 1)
		return (0);
	if (strncmp(name, "band_", sizeof("band_") - 1) != 0)
		return (0);
	if (strcmp(name + namelen - 5, ".json") != 0)
		return (0);
	vtc_log(mbe_vl, 2, "Found enrollment: %s/%s", band_confdir_enroll,
	    name);
	arg->n_enroll++;
	return (0);
}

static json_t *
mbe_band_read(const char *filename)
{
	json_t *jroot, *juuid, *jband_name, *jband_jwt;
	json_error_t jerror;
	char filepath[ODR_BUFSIZ];

	ODR_snprintf(filepath, sizeof(filepath), "%s/%s",
	    band_confdir_enroll, filename);
	if (ODR_access(filepath, ODR_ACCESS_F_OK) == -1) {
		vtc_log(vl, 0, "BANDEC_00582: File not found: %s",
		    filepath);
		return (NULL);
	}
	jroot = json_load_file(filepath, 0, &jerror);
	if (jroot == NULL) {
		vtc_log(vl, 1,
		    "BANDEC_00583: error while reading JSON format:"
		    " on line %d: %s", jerror.line, jerror.text);
		return (NULL);
	}
	juuid = json_object_get(jroot, "uuid");
	AN(juuid);
	assert(json_is_string(juuid));
	jband_name = json_object_get(jroot, "name");
	AN(jband_name);
	assert(json_is_string(jband_name));
	jband_jwt = json_object_get(jroot, "jwt");
	AN(jband_jwt);
	assert(json_is_string(jband_jwt));
	return (jroot);
}

int
MBE_get_enrollment_count(void)
{
	struct mbe_traversal_dir_arg arg = { 0, };
	int r;

	r = ODR_traversal_dir(vl,
	    band_confdir_enroll, mbe_traversal_dir_callback, &arg);
	if (r != 0) {
		vtc_log(vl, 0, "BANDEC_00584: ODR_traversal_dir() failed");
		return (-1);
	}
	return (arg.n_enroll);
}

json_t *
MBE_get_active_band(void)
{
	json_t *active_band;
	const char *default_band_uuid;
	char filepath[ODR_BUFSIZ];

	default_band_uuid = MPC_get_default_band_uuid();
	if (default_band_uuid == NULL) {
		vtc_log(vl, 0,
		    "BANDEC_00611: MPC_get_default_band_uuid() failed");
		return (NULL);
	}
	ODR_snprintf(filepath, sizeof(filepath), "band_%s.json",
	    default_band_uuid);
	active_band = mbe_band_read(filepath);
	if (active_band == NULL) {
		vtc_log(vl, 0,
		    "BANDEC_00612: mbe_band_read() failed");
		return (NULL);
	}
	return (active_band);
}

static int
mbe_get_enrollment_list_dir_callback(struct vtclog *mbe_vl, const char *name,
    void *orig_arg)
{
	struct mbe_traversal_dir_arg *arg =
	    (struct mbe_traversal_dir_arg *)orig_arg;
	json_t *enrollment, *jband, *jband_name;
	int namelen;
	char uuidstr[64];

	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		return (0);
	namelen = strlen(name);
	if (namelen < sizeof("band_0b0a3721-7dc0-4391-969d-b3b0d1e00925.json") - 1)
		return (0);
	if (strncmp(name, "band_", sizeof("band_") - 1) != 0)
		return (0);
	if (strcmp(name + namelen - 5, ".json") != 0)
		return (0);
	vtc_log(mbe_vl, 2, "Found enrollment: %s/%s", band_confdir_enroll,
	    name);
	ODR_snprintf(uuidstr, sizeof(uuidstr),
	    "%.*s", namelen - 5 - (sizeof("band_") - 1),
	    name + sizeof("band_") - 1);
	jband = mbe_band_read(name);
	if (jband == NULL) {
		vtc_log(vl, 0, "BANDEC_00613: mbe_band_read() failed");
		return (0);
	}
	jband_name = json_object_get(jband, "name");
	AN(jband_name);
	assert(json_is_string(jband_name));
	enrollment = json_object();
	AN(enrollment);
	json_object_set_new(enrollment, "band_uuid", json_string(uuidstr));
	json_object_set_new(enrollment, "name", json_string(json_string_value(jband_name)));
	json_array_append_new(arg->jroot, enrollment);
	json_decref(jband);

	return (0);
}

json_t *
MBE_get_enrollment_list(void)
{
	struct mbe_traversal_dir_arg arg = { 0, };
	int r;

	arg.jroot = json_array();
	AN(arg.jroot);

	r = ODR_traversal_dir(vl,
	    band_confdir_enroll, mbe_get_enrollment_list_dir_callback, &arg);
	if (r != 0) {
		vtc_log(vl, 0, "BANDEC_00614: ODR_traversal_dir() failed");
		return (NULL);
	}
	return (arg.jroot);
}

int
MBE_unenroll(const char *band_uuid)
{
	char filepath[ODR_BUFSIZ];

	ODR_snprintf(filepath, sizeof(filepath), "%s/band_%s.json",
	    band_confdir_enroll, band_uuid);
	mbe_file_delete(filepath);
	ODR_snprintf(filepath, sizeof(filepath), "%s/conf_%s.json",
	    band_confdir_enroll, band_uuid);
	mbe_file_delete(filepath);
	ODR_snprintf(filepath, sizeof(filepath), "%s/admin_%s.json",
	    band_confdir_admin, band_uuid);
	mbe_file_delete(filepath);
	MPC_remove_default_band_uuid();
	return (0);
}
