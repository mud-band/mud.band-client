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

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "odr.h"
#include "vassert.h"
#include "vtc_log.h"
#include "vuuid.h"

#include "mudband.h"

static vuuid_t mbe_default_uuid;
static char mbe_default_uuidstr[64];
static struct vtclog *mbe_vl;
json_t *mbe_jroot;

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
        vtc_log(mbe_vl, 0, "BANDEC_00229: Failed to open file %s: %s",
                filepath, strerror(errno));
        return (-1);
    }
    r = json_dumpf(obj, fp, 0);
    if (r == -1) {
        vtc_log(mbe_vl, 0,
                "BANDEC_00230: Failed to write JSON to file %s: %s",
                filepath, strerror(errno));
        fclose(fp);
        return (-1);
    }
    fclose(fp);
    return (0);
}

const vuuid_t *
MBE_get_uuid(void)
{

    return (&mbe_default_uuid);
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

int
MBE_parse_unenroll_response(const char *body)
{
    json_t *jroot, *jstatus;
    json_error_t jerror;
    int r, status;
    char filepath[ODR_BUFSIZ];
    const char *default_band_uuid;

    jroot = json_loads(body, 0, &jerror);
    if (jroot == NULL) {
        vtc_log(mbe_vl, 1,
                "BANDEC_00231: error while parsing JSON format:"
                " on line %d: %s", jerror.line, jerror.text);
        vtc_log(mbe_vl, 1,
                "BANDEC_00232: response body: %s", body);
        return (-1);
    }
    jstatus = json_object_get(jroot, "status");
    AN(jstatus);
    assert(json_is_integer(jstatus));
    status = json_integer_value(jstatus);
    if (status != 200) {
        json_t *jmsg;

        if (status == 505 /* No band found */ ||
            status == 506 /* No device found */) {
            goto unenroll_force;
        }

        jmsg = json_object_get(jroot, "msg");
        AN(jmsg);
        assert(json_is_string(jmsg));
        vtc_log(mbe_vl, 1,
                "BANDEC_00233: Failed to unenroll. (reason %s)",
                json_string_value(jmsg));
        json_decref(jroot);
        return (-1);
    }
unenroll_force:
    json_decref(jroot);

    default_band_uuid = MPC_get_default_band_uuid();
    MPC_delete_default_band_uuid();
    snprintf(filepath, sizeof(filepath), "%s/band_%s.json",
             band_enroll_dir, default_band_uuid);
    mbe_file_delete(filepath);
    snprintf(filepath, sizeof(filepath), "%s/conf_%s.json",
             band_enroll_dir, default_band_uuid);
    mbe_file_delete(filepath);
    if (mbe_jroot != NULL) {
        json_decref(mbe_jroot);
        mbe_jroot = NULL;
    }
    return (0);
}

int
MBE_parse_enroll_response(const char *private_key, const char *body)
{
    json_t *jroot, *jstatus, *jband, *juuid, *jband_name;
    json_error_t jerror;
    int r, status;
    char filepath[ODR_BUFSIZ];

    jroot = json_loads(body, 0, &jerror);
    if (jroot == NULL) {
        vtc_log(mbe_vl, 1,
                "BANDEC_00234: error while parsing JSON format:"
                " on line %d: %s", jerror.line, jerror.text);
        vtc_log(mbe_vl, 1,
                "BANDEC_00235: response body: %s", body);
        return (-1);
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
                "BANDEC_00236: Failed to enroll. (reason %s)",
                json_string_value(jmsg));
        json_decref(jroot);
        return (-1);
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
    json_object_set_new(jband, "wireguard_privkey", json_string(private_key));
    ODR_snprintf(filepath, sizeof(filepath), "%s/band_%s.json",
                 band_enroll_dir, json_string_value(juuid));
    r = mbe_file_write(filepath, jband);
    assert(r == 0);
    MPC_set_default_band_uuid(json_string_value(juuid));
    vtc_log(mbe_vl, 2, "Enrolled in the band: %s (uuid %s)",
            json_string_value(jband_name), json_string_value(juuid));
    /* delete the previous cached config. */
    ODR_snprintf(filepath, sizeof(filepath), "%s/conf_%s.json",
                 band_enroll_dir, json_string_value(juuid));
    mbe_file_delete(filepath);
    json_decref(jroot);
    /* reload the band information if possible. */
    r = MBE_check_and_read();
    if (r != 0) {
        vtc_log(mbe_vl, 1, "BANDEC_00237: MBE_check_and_read() failed.");
        return (-1);
    }
    return (0);
}

struct mbe_traversal_dir_arg {
    int	n_enroll;
    int	b_arg_found;
    char	b_arg_uuidstr[64];
};

static json_t *
mbe_band_read(const char *filename)
{
    json_t *jroot, *juuid, *jband_name, *jband_jwt;
    json_error_t jerror;
    char filepath[ODR_BUFSIZ];

    ODR_snprintf(filepath, sizeof(filepath), "%s/%s",
                 band_enroll_dir, filename);
    if (ODR_access(filepath, ODR_ACCESS_F_OK) == -1) {
        vtc_log(mbe_vl, 0, "BANDEC_00238: File not found: %s",
                filepath);
        return (NULL);
    }
    jroot = json_load_file(filepath, 0, &jerror);
    if (jroot == NULL) {
        vtc_log(mbe_vl, 1,
                "BANDEC_00239: error while reading JSON format:"
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
MBE_get_band_name_by_uuid(const char *uuid, char *buf, size_t bufmax)
{
    json_t *jroot, *jname;
    char filename[ODR_BUFSIZ];

    ODR_snprintf(filename, sizeof(filename), "band_%s.json", uuid);
    jroot = mbe_band_read(filename);
    if (jroot == NULL)
        return (-1);
    jname = json_object_get(jroot, "name");
    AN(jname);
    assert(json_is_string(jname));
    assert(json_string_length(jname) > 0);
    ODR_snprintf(buf, bufmax, "%s", json_string_value(jname));
    json_decref(jroot);
    return (0);
}

int
MBE_get_band_name(char *buf, size_t bufmax)
{
    json_t *jname;
    int r;

    if (mbe_jroot == NULL) {
        r = MBE_check_and_read();
        if (r != 0) {
            vtc_log(mbe_vl, 1, "BANDEC_00240: MBE_check_and_read() failed.");
            return (-1);
        }
    }
    AN(mbe_jroot);
    jname = json_object_get(mbe_jroot, "name");
    AN(jname);
    assert(json_is_string(jname));
    assert(json_string_length(jname) > 0);
    ODR_snprintf(buf, bufmax, "%s", json_string_value(jname));
    return (0);
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

static int
mbe_traversal_dir_callback(struct vtclog *vl, const char *name, void *orig_arg)
{
    struct mbe_traversal_dir_arg *arg =
            (struct mbe_traversal_dir_arg *)orig_arg;
    int namelen;
    char bandname[64];
    const char *p;
    const char *default_band_uuid;

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
            band_enroll_dir, name, bandname);
    arg->n_enroll++;
    default_band_uuid = MPC_get_default_band_uuid();
    if (default_band_uuid != NULL) {
        p = ODR_strcasestr(mbe_default_uuidstr, default_band_uuid);
        if (p != NULL) {
            arg->b_arg_found = 1;
            ODR_snprintf(arg->b_arg_uuidstr,
                         sizeof(arg->b_arg_uuidstr), "%s",
                         mbe_default_uuidstr);
            vtc_log(mbe_vl, 2,
                    "Found matched enrollment: %s (%s)",
                    mbe_default_uuidstr, default_band_uuid);
        }
    }
    return (0);
}

const char *
MBE_get_jwt(void)
{
    json_t *jband_jwt;

    AN(mbe_jroot);
    jband_jwt = json_object_get(mbe_jroot, "jwt");
    if (jband_jwt == NULL)
        return (NULL);
    assert(json_is_string(jband_jwt));
    assert(json_string_length(jband_jwt) > 0);
    return (json_string_value(jband_jwt));
}

int
MBE_is_public(void)
{
    json_t *jopt_public;

    if (mbe_jroot == NULL)
        return (0);
    jopt_public = json_object_get(mbe_jroot, "opt_public");
    if (jopt_public == NULL)
        return (0);
    assert(json_is_integer(jopt_public));
    return ((int)json_integer_value(jopt_public));
}

int
MBE_check_and_read(void)
{
    struct mbe_traversal_dir_arg dir_arg = { 0, };
    uint32_t status;
    int r;
    char filename[64];

    r = ODR_traversal_dir(mbe_vl, band_enroll_dir,
                          mbe_traversal_dir_callback, (void *)&dir_arg);
    if (r != 0) {
        vtc_log(mbe_vl, 0, "BANDEC_00241: ODR_traversal_dir() failed");
        return (-1);
    }
    if (dir_arg.n_enroll == 0) {
        vtc_log(mbe_vl, 2, "No enrollments found.");
        return (0);
    }
    if (!dir_arg.b_arg_found) {
        vtc_log(mbe_vl, 1,
                "BANDEC_00242: Enrollment for the band ID %d not found.",
                MPC_get_default_band_uuid());
        return (-1);
    }
    ODR_snprintf(mbe_default_uuidstr, sizeof(mbe_default_uuidstr), "%s", dir_arg.b_arg_uuidstr);
    assert(mbe_default_uuidstr[0] != '\0');
    status = vuuid_s_ok;
    VUUID_from_string(mbe_default_uuidstr, &mbe_default_uuid, &status);
    assert(status == vuuid_s_ok);
    vtc_log(mbe_vl, 2, "Selected the enrollment for band uuid %s",
            mbe_default_uuidstr);
    ODR_snprintf(filename, sizeof(filename), "band_%s.json",
                 mbe_default_uuidstr);
    if (mbe_jroot != NULL) {
        json_decref(mbe_jroot);
    }
    mbe_jroot = mbe_band_read(filename);
    AN(mbe_jroot);
    return (0);
}

int
MBE_init(void)
{

    mbe_vl = vtc_logopen("enroll", mudband_log_printf);
    AN(mbe_vl);
    return (0);
}
