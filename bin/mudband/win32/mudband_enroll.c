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
#include "vtc_log.h"
#include "vuuid.h"

#include "mudband.h"
#include "mudband_stun_client.h"

static vuuid_t mbe_default_uuid;
static char mbe_default_uuidstr[64];
static struct vtclog *mbe_vl;
json_t *mbe_jroot;

static void	mbe_get_band_name_from_filepath(const char *filename,
		    char *buf, size_t bufmax);
static json_t *	mbe_band_read(const char *filename);

static void
mbe_file_delete(const char *filepath)
{
	int r;

	r = ODR_unlink(filepath);
	assert(r == 0 || (r == -1 && errno == ENOENT));
}

const char *
MBE_get_private_key(void)
{
	json_t *wireguard_privkey;

	AN(mbe_jroot);
	wireguard_privkey = json_object_get(mbe_jroot, "wireguard_privkey");
	AN(wireguard_privkey);
	assert(json_is_string(wireguard_privkey));
	assert(json_string_length(wireguard_privkey) > 0);
	return (json_string_value(wireguard_privkey));
}

const vuuid_t *
MBE_get_uuid(void)
{

	return (&mbe_default_uuid);
}

const char *
MBE_get_uuidstr(void)
{

	return (mbe_default_uuidstr);
}

static int
mbe_file_write(const char *filepath, json_t *obj)
{
	FILE *fp;
	int r;

	fp = fopen(filepath, "w+");
	if (fp == NULL) {
		vtc_log(mbe_vl, 0, "BANDEC_00094: Failed to open file %s: %s",
		    filepath, strerror(errno));
		return (-1);
	}
	r = json_dumpf(obj, fp, 0);
	if (r == -1) {
		vtc_log(mbe_vl, 0,
		    "BANDEC_00095: Failed to write JSON to file %s: %s",
		    filepath, strerror(errno));
		fclose(fp);
		return (-1);
	}
	fclose(fp);
	return (0);
}

int
MBE_enroll(const char *token, const char *name, const char *secret)
{
	struct vhttps_req req;
	json_t *jroot, *jstatus, *jband, *juuid, *jband_name, *jopt_public;
	json_error_t jerror;
	size_t resp_bodylen, wg_pubkeystrlen, wg_privkeystrlen;
	int r, req_bodylen, status;
	char req_body[ODR_BUFSIZ], resp_body[ODR_BUFSIZ];
	char filepath[ODR_BUFSIZ];
	uint8_t wg_privkey[WIREGUARD_PRIVATE_KEY_LEN];
	uint8_t wg_pubkey[WIREGUARD_PUBLIC_KEY_LEN];
	char wg_pubkeystr[WIREGUARD_PUBLIC_KEY_LEN * 2 + 1 /* XXX */];
	char wg_privkeystr[WIREGUARD_PRIVATE_KEY_LEN * 2 + 1 /* XXX */];
	bool success;

	if (name == NULL) {
		vtc_log(mbe_vl, 0,
		    "[ERROR] BANDEC_00504: Missing -n argument."
		    " Specify the device name.");
		return (1);
	}

	vtc_log(mbe_vl, 2, "Enrolling with token: %s (name %s)", token, name);
	/* generate wireguard key pair */
	wireguard_generate_private_key(wg_privkey);
	wireguard_generate_public_key(wg_pubkey, wg_privkey);
	wg_pubkeystrlen = sizeof(wg_pubkeystr);
	success = wireguard_base64_encode(wg_pubkey, sizeof(wg_pubkey),
	    wg_pubkeystr, &wg_pubkeystrlen);
	if (!success) {
		vtc_log(mbe_vl, 0,
		    "BANDEC_00096: wireguard_base64_encode() failed.");
		return (1);
	}
	wg_privkeystrlen = sizeof(wg_privkeystr);
	success = wireguard_base64_encode(wg_privkey, sizeof(wg_privkey),
	    wg_privkeystr, &wg_privkeystrlen);
	if (!success) {
		vtc_log(mbe_vl, 0,
		    "BANDEC_00097: wireguard_base64_encode() failed.");
		return (1);
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
	req.vl = mbe_vl;
	req.server = "www.mud.band:443";
	req.domain = "www.mud.band";
	req.url = "/api/band/enroll";
	req.hdrs = "Content-Type: application/json\r\n"
	    "Host: www.mud.band\r\n";
	req.body = req_body;
	req.bodylen = req_bodylen;

	resp_bodylen = sizeof(resp_body);
	r = VHTTPS_post(&req, resp_body, &resp_bodylen);
	if (r == -1) {
		vtc_log(mbe_vl, 0, "BANDEC_00098: VHTTPS_post() failed.");
		return (1);
	}
	assert(resp_bodylen >= 0);
	resp_body[resp_bodylen] = '\0';

	jroot = json_loads(resp_body, 0, &jerror);
	if (jroot == NULL) {
		vtc_log(mbe_vl, 1,
		    "BANDEC_00099: error while parsing JSON format:"
		    " on line %d: %s", jerror.line, jerror.text);
		vtc_log(mbe_vl, 1,
		    "BANDEC_00100: response body: %s", resp_body);
		return (1);
	}
	jstatus = json_object_get(jroot, "status");
	AN(jstatus);
	assert(json_is_integer(jstatus));
	status = json_integer_value(jstatus);
	if (status != 200) {
		json_t *jmsg;

		jmsg = json_object_get(jroot, "msg");
		AN(jmsg);
		assert(json_is_string(jmsg));
		vtc_log(mbe_vl, 1,
		    "BANDEC_00101: Failed to enroll. (reason %s)",
		    json_string_value(jmsg));
		json_decref(jroot);
		return (1);	
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
	vtc_log(mbe_vl, 2, "Enrolled in the band: %s (uuid %s)",
	    json_string_value(jband_name), json_string_value(juuid));
	if (json_integer_value(jopt_public) != 0) {
		vtc_log(mbe_vl, 2, "NOTE: This band is public. This means that");
		vtc_log(mbe_vl, 2, "* Nobody can connect to your device without your permission.");
		vtc_log(mbe_vl, 2, "* Your default policy is 'block'.");
		vtc_log(mbe_vl, 2, "  You can change the default policy by using the following command:");
		vtc_log(mbe_vl, 2, "  $ mudband --acl-default-policy allow|block");
		vtc_log(mbe_vl, 2, "* You need to add an ACL rule to allow the connection.");
		vtc_log(mbe_vl, 2, "* You can add the ACL rule by using the following command:");
		vtc_log(mbe_vl, 2, "  $ mudband --acl-add <syntax>");
		vtc_log(mbe_vl, 2, "* For details, please visit https://mud.band/docs/public-band link.");
	} else {
		vtc_log(mbe_vl, 2, "NOTE: This band is private. This means that");
		vtc_log(mbe_vl, 2, "* Band admin only can control ACL rules and the default policy.");
		vtc_log(mbe_vl, 2, "* You can't control your device.");
		vtc_log(mbe_vl, 2, "* For details, please visit https://mud.band/docs/private-band link.");
	}
	/* delete the previous cached config. */
	ODR_snprintf(filepath, sizeof(filepath), "%s/conf_%s.json",
	    band_confdir_enroll, json_string_value(juuid));
	mbe_file_delete(filepath);
	json_decref(jroot);
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
		vtc_log(mbe_vl, 0, "BANDEC_00102: File not found: %s",
		    filepath);
		return (NULL);
	}
	jroot = json_load_file(filepath, 0, &jerror);
	if (jroot == NULL) {
		vtc_log(mbe_vl, 1,
		    "BANDEC_00103: error while reading JSON format:"
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

static void
mbe_get_band_name_from_filepath(const char *filename, char *buf, size_t bufmax)
{
	json_t *jroot, *jname;

	jroot = mbe_band_read(filename);
	if (jroot == NULL)
		return;
	AN(jroot);
	assert(json_is_object(jroot));
	jname = json_object_get(jroot, "name");
	AN(jname);
	assert(json_is_string(jname));
	assert(json_string_length(jname) > 0);
	ODR_snprintf(buf, bufmax, "%s", json_string_value(jname));
	json_decref(jroot);
}


struct mbe_traversal_dir_arg {
	int	n_enroll;
	int	b_arg_found;
	char	b_arg_uuidstr[64];
};

static int
mbe_traversal_dir_callback(struct vtclog *vl, const char *name, void *orig_arg)
{
	struct mbe_traversal_dir_arg *arg =
	    (struct mbe_traversal_dir_arg *)orig_arg;
	int namelen;
	char bandname[64];
	const char *p;

	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		return (0);
	namelen = strlen(name);
	if (namelen < sizeof("band_0b0a3721-7dc0-4391-969d-b3b0d1e00925.json") - 1)
		return (0);
	if (strncmp(name, "band_", sizeof("band_") - 1) != 0)
		return (0);
	if (strcmp(name + namelen - 5, ".json") != 0)
		return (0);
	ODR_snprintf(mbe_default_uuidstr, sizeof(mbe_default_uuidstr),
	    "%.*s", namelen - 5 - (sizeof("band_") - 1),
	    name + sizeof("band_") - 1);
	bandname[0] = '\0';
	mbe_get_band_name_from_filepath(name, bandname, sizeof(bandname));
	vtc_log(vl, 2, "Found enrollment: %s/%s (band_name %s)",
	    band_confdir_enroll, name, bandname);
	arg->n_enroll++;
	if (band_b_arg != NULL) {
		p = ODR_strcasestr(mbe_default_uuidstr, band_b_arg);
		if (p != NULL) {
			arg->b_arg_found = 1;
			ODR_snprintf(arg->b_arg_uuidstr,
			    sizeof(arg->b_arg_uuidstr), "%s",
			    mbe_default_uuidstr);
			vtc_log(mbe_vl, 2,
			    "Found matched enrollment: %s (%s)",
			    mbe_default_uuidstr, band_b_arg);
		}
	}
	return (0);
}

int
MBE_check_and_read(void)
{
	struct mbe_traversal_dir_arg dir_arg = { 0, };
	uint32_t status;
	int r;
	char filename[64];

	r = ODR_traversal_dir(mbe_vl, band_confdir_enroll,
	    mbe_traversal_dir_callback, (void *)&dir_arg);
	if (r != 0) {
		vtc_log(mbe_vl, 0, "BANDEC_00104: ODR_traversal_dir() failed");
		return (-1);
	}
	if (dir_arg.n_enroll == 0) {
		vtc_log(mbe_vl, 0, "BANDEC_00105: No enrollments found.");
		return (-1);
	}
	if (dir_arg.n_enroll > 1 && band_b_arg == NULL) {
		const char *default_band_uuid;

		default_band_uuid = MPC_get_default_band_uuid();
		if (default_band_uuid == NULL) {
			vtc_log(mbe_vl, 1,
			    "BANDEC_00106: Multiple enrollments found."
			    " Use -b to select.");
			return (-1);
		}
		ODR_snprintf(mbe_default_uuidstr, sizeof(mbe_default_uuidstr),
		    "%s", default_band_uuid);
	}
	if (band_b_arg != NULL) {
		if (!dir_arg.b_arg_found) {
			vtc_log(mbe_vl, 1,
			    "BANDEC_00107: Enrollment for the band ID %s"
			    " not found.", band_b_arg);
			return (-1);
		}
		ODR_snprintf(mbe_default_uuidstr, sizeof(mbe_default_uuidstr),
		    "%s", dir_arg.b_arg_uuidstr);
	}
	assert(mbe_default_uuidstr[0] != '\0');
	status = vuuid_s_ok;
	VUUID_from_string(mbe_default_uuidstr, &mbe_default_uuid, &status);
	assert(status == vuuid_s_ok);
	vtc_log(mbe_vl, 2, "Selected the enrollment for band uuid %s",
	    mbe_default_uuidstr);
	ODR_snprintf(filename, sizeof(filename), "band_%s.json",
	    mbe_default_uuidstr);
	mbe_jroot = mbe_band_read(filename);
	AN(mbe_jroot);
	return (0);
}

int
MBE_list(void)
{
	struct mbe_traversal_dir_arg dir_arg = { 0, };
	int r;

	r = ODR_traversal_dir(mbe_vl, band_confdir_enroll,
	    mbe_traversal_dir_callback, (void *)&dir_arg);
	if (r != 0) {
		vtc_log(mbe_vl, 0, "BANDEC_00501: ODR_traversal_dir() failed");
		return (1);
	}
	if (dir_arg.n_enroll == 0) {
		vtc_log(mbe_vl, 0, "BANDEC_00502: No enrollments found.");
		return (1);
	}
	return (0);
}

void
MBE_fini(void)
{

	if (mbe_jroot != NULL)
		json_decref(mbe_jroot);
	if (mbe_vl != NULL)
		vtc_logclose(mbe_vl);
}

int
MBE_init(void)
{

	mbe_vl = vtc_logopen("enroll", NULL);
	AN(mbe_vl);
	return (0);
}
