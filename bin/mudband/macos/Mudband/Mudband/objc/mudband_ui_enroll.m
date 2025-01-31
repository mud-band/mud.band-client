//
// Copyright (c) 2024 Weongyo Jeong <weongyo@gmail.com>
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
//

#import <Foundation/Foundation.h>

#include "jansson.h"
#include "odr.h"
#include "vtc_log.h"
#include "vuuid.h"

#include "libwireguard.h"
#include "mudband_ui.h"

static struct vtclog *enroll_vl;
static json_t *enroll_jroot;
static vuuid_t enroll_default_uuid;
static char enroll_default_uuidstr[64];

static json_t *
mudband_ui_enroll_band_read(const char *filename)
{
    json_t *jroot, *juuid, *jband_name, *jband_jwt;
    json_error_t jerror;
    char filepath[ODR_BUFSIZ];

    snprintf(filepath, sizeof(filepath), "%s/%s", band_enroll_dir, filename);
    if (access(filepath, F_OK) == -1) {
        vtc_log(enroll_vl, 0, "BANDEC_00392: File not found: %s",
            filepath);
        return (NULL);
    }
    jroot = json_load_file(filepath, 0, &jerror);
    if (jroot == NULL) {
        vtc_log(enroll_vl, 1,
            "BANDEC_00393: error while reading JSON format:"
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
mudband_ui_enroll_get_band_name_from_filepath(const char *filename, char *buf, size_t bufmax)
{
    json_t *jroot, *jname;

    jroot = mudband_ui_enroll_band_read(filename);
    if (jroot == NULL)
        return;
    AN(jroot);
    assert(json_is_object(jroot));
    jname = json_object_get(jroot, "name");
    AN(jname);
    assert(json_is_string(jname));
    assert(json_string_length(jname) > 0);
    snprintf(buf, bufmax, "%s", json_string_value(jname));
    json_decref(jroot);
}

NSString *
mudband_ui_enroll_get_band_name(void)
{
    json_t *jname;
    
    AN(enroll_jroot);
    jname = json_object_get(enroll_jroot, "name");
    AN(jname);
    assert(json_is_string(jname));
    assert(json_string_length(jname) > 0);
    return [NSString stringWithUTF8String:json_string_value(jname)];
}

NSString *
mudband_ui_enroll_get_band_uuid(void)
{
    const char *p;
    
    p = mudband_ui_progconf_get_default_band_uuid();
    if (p == NULL)
        return (NULL);
    return [NSString stringWithUTF8String:p];
}

bool
mudband_ui_enroll_is_public(void)
{
    json_t *jopt_public;
    
    if (enroll_jroot == NULL)
        return (false);
    jopt_public = json_object_get(enroll_jroot, "opt_public");
    AN(jopt_public);
    assert(json_is_integer(jopt_public));
    return (json_integer_value(jopt_public) == 1);
}

static int
mudband_ui_enroll_get_band_uuidstraversal_dir_callback(struct vtclog *vl,
                                                   const char *name,
                                                   void *orig_arg)
{
    struct mudband_ui_traversal_dir_arg *arg =
      (struct mudband_ui_traversal_dir_arg *)orig_arg;
    NSMutableArray *mutableArray = (__bridge NSMutableArray *)arg->arg;
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
    [mutableArray addObject:[NSString stringWithUTF8String:band_uuidstr]];
    arg->n_enroll++;
    return (0);
}

NSMutableArray *
mudband_ui_enroll_get_band_uuids(void)
{
    struct mudband_ui_traversal_dir_arg dir_arg = { 0, };
    NSMutableArray *mutableArray = [[NSMutableArray alloc] init];
    int r;
    
    dir_arg.arg = (__bridge void *)mutableArray;
    r = ODR_traversal_dir(enroll_vl, band_enroll_dir,
                          mudband_ui_enroll_get_band_uuidstraversal_dir_callback,
                          (void *)&dir_arg);
    if (r != 0) {
        vtc_log(enroll_vl, 0, "BANDEC_00394: ODR_traversal_dir() failed");
        [mutableArray removeAllObjects];
        return (NULL);
    }
    if (dir_arg.n_enroll == 0) {
        return (mutableArray);
    }
    return (mutableArray);
}

static int
mudband_ui_enroll_get_count_traversal_dir_callback(struct vtclog *vl,
                                                   const char *name,
                                                   void *orig_arg)
{
    struct mudband_ui_traversal_dir_arg *arg =
        (struct mudband_ui_traversal_dir_arg *)orig_arg;
    int namelen;
    char bandname[64];

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return (0);
    namelen = (int)strlen(name);
    if (namelen < sizeof("band_0b0a3721-7dc0-4391-969d-b3b0d1e00925.json") - 1)
        return (0);
    if (strncmp(name, "band_", sizeof("band_") - 1) != 0)
        return (0);
    if (strcmp(name + namelen - 5, ".json") != 0)
        return (0);
    bandname[0] = '\0';
    mudband_ui_enroll_get_band_name_from_filepath(name, bandname, sizeof(bandname));
    vtc_log(vl, 2, "Found enrollment: %s/%s (band_name %s)",
            band_enroll_dir, name, bandname);
    arg->n_enroll++;
    return (0);
}

int
mudband_ui_enroll_get_count(void)
{
    struct mudband_ui_traversal_dir_arg dir_arg = { 0, };
    int r;
    
    r = ODR_traversal_dir(enroll_vl, band_enroll_dir,
                          mudband_ui_enroll_get_count_traversal_dir_callback,
                          (void *)&dir_arg);
    if (r != 0) {
        vtc_log(enroll_vl, 0, "BANDEC_00395: ODR_traversal_dir() failed");
        return (-1);
    }
    if (dir_arg.n_enroll == 0) {
        vtc_log(enroll_vl, 0, "BANDEC_00396: No enrollments found.");
        return (0);
    }
    return (dir_arg.n_enroll);
}

int
mudband_ui_enroll_load(void)
{
    uint32_t status;
    const char *default_band_uuid;
    char filename[64];
    
    default_band_uuid = mudband_ui_progconf_get_default_band_uuid();
    if (default_band_uuid == NULL) {
        vtc_log(enroll_vl, 0, "BANDEC_00397: No default band UUID found.");
        return (-1);
    }
    snprintf(enroll_default_uuidstr, sizeof(enroll_default_uuidstr),
             "%s", default_band_uuid);
    assert(enroll_default_uuidstr[0] != '\0');
    status = vuuid_s_ok;
    VUUID_from_string(enroll_default_uuidstr, &enroll_default_uuid, &status);
    assert(status == vuuid_s_ok);
    vtc_log(enroll_vl, 2, "Selected the enrollment for band uuid %s",
        enroll_default_uuidstr);
    snprintf(filename, sizeof(filename), "band_%s.json",
        enroll_default_uuidstr);
    if (enroll_jroot != NULL)
        json_decref(enroll_jroot);
    enroll_jroot = mudband_ui_enroll_band_read(filename);
    AN(enroll_jroot);
    return (0);
}


static int
mudband_ui_enroll_file_write(const char *filepath, json_t *obj)
{
    FILE *fp;
    int r;

    fp = fopen(filepath, "w+");
    if (fp == NULL) {
        vtc_log(enroll_vl, 0, "BANDEC_00398: Failed to open file %s: %s",
            filepath, strerror(errno));
        return (-1);
    }
    r = json_dumpf(obj, fp, 0);
    if (r == -1) {
        vtc_log(enroll_vl, 0,
            "BANDEC_00399: Failed to write JSON to file %s: %s",
            filepath, strerror(errno));
        fclose(fp);
        return (-1);
    }
    fclose(fp);
    return (0);
}

static void
mudband_ui_enroll_file_delete(const char *filepath)
{
    int r;

    r = unlink(filepath);
    assert(r == 0 || (r == -1 && errno == ENOENT));
}

int
mudband_ui_enroll_post(NSString *priv_key, NSString *raw_str)
{
    json_t *jroot, *jband, *juuid, *jband_name;
    json_error_t jerror;
    int r;
    const char *priv_keyptr = [priv_key UTF8String];
    const char *resp_body = [raw_str UTF8String];
    char filepath[ODR_BUFSIZ];

    jroot = json_loads(resp_body, 0, &jerror);
    if (jroot == NULL) {
        vtc_log(enroll_vl, 1,
            "BANDEC_00400: error while parsing JSON format:"
            " on line %d: %s", jerror.line, jerror.text);
        vtc_log(enroll_vl, 1,
            "BANDEC_00401: response body: %s", resp_body);
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
    json_object_set_new(jband, "wireguard_privkey",
        json_string(priv_keyptr));
    snprintf(filepath, sizeof(filepath), "%s/band_%s.json",
             band_enroll_dir, json_string_value(juuid));
    r = mudband_ui_enroll_file_write(filepath, jband);
    assert(r == 0);
    vtc_log(enroll_vl, 2, "Enrolled in the band: %s (uuid %s)",
        json_string_value(jband_name), json_string_value(juuid));
    mudband_ui_progconf_set_default_band_uuid(json_string_value(juuid));
    /* delete the previous cached config. */
    snprintf(filepath, sizeof(filepath), "%s/conf_%s.json",
             band_enroll_dir, json_string_value(juuid));
    mudband_ui_enroll_file_delete(filepath);
    json_decref(jroot);
    return (0);
}

NSString *
mudband_ui_enroll_get_jwt(void)
{
    json_t *jband_jwt;
    
    AN(enroll_jroot);
    jband_jwt = json_object_get(enroll_jroot, "jwt");
    if (jband_jwt == NULL)
        return (NULL);
    assert(json_is_string(jband_jwt));
    assert(json_string_length(jband_jwt) > 0);
    return [NSString stringWithUTF8String:json_string_value(jband_jwt)];
}

void
mudband_ui_enroll_unenroll(NSString *band_uuid)
{
    const char *band_uuidp = [band_uuid UTF8String];
    char filepath[ODR_BUFSIZ];
    
    mudband_ui_progconf_delete_default_band_uuid();
    snprintf(filepath, sizeof(filepath), "%s/band_%s.json",
             band_enroll_dir, band_uuidp);
    mudband_ui_enroll_file_delete(filepath);
    snprintf(filepath, sizeof(filepath), "%s/conf_%s.json",
             band_enroll_dir, band_uuidp);
    mudband_ui_enroll_file_delete(filepath);
    if (enroll_jroot != NULL) {
        json_decref(enroll_jroot);
        enroll_jroot = NULL;
    }
}

void
mudband_ui_enroll_init(void)
{
    
    enroll_vl = vtc_logopen("enroll", NULL);
    AN(enroll_vl);
}
