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

#include "mudband_ui.h"

static struct vtclog *progconf_vl;
static json_t *progconf_jroot;

static json_t *
mudband_ui_progconf_read(void)
{
    json_t *jroot;
    json_error_t jerror;
    char filepath[ODR_BUFSIZ];

    snprintf(filepath, sizeof(filepath), "%s/mudband.conf", band_top_dir);
    if (access(filepath, F_OK) == -1) {
        vtc_log(progconf_vl, 0, "BANDEC_00265: File not found: %s",
                filepath);
        return (NULL);
    }
    jroot = json_load_file(filepath, 0, &jerror);
    if (jroot == NULL) {
        vtc_log(progconf_vl, 1,
                "BANDEC_00266: error while reading JSON format:"
                " on line %d: %s", jerror.line, jerror.text);
        return (NULL);
    }
    return (jroot);
}

static int
mudband_ui_progconf_write(void)
{
    FILE *fp;
    int r;
    char filepath[ODR_BUFSIZ];

    AN(progconf_jroot);
    snprintf(filepath, sizeof(filepath), "%s/mudband.conf", band_top_dir);
    fp = fopen(filepath, "w+");
    if (fp == NULL) {
        vtc_log(progconf_vl, 0, "BANDEC_00267: Failed to open file %s: %s",
                filepath, strerror(errno));
        return (-1);
    }
    r = json_dumpf(progconf_jroot, fp, 0);
    if (r == -1) {
        vtc_log(progconf_vl, 0,
                "BANDEC_00268: Failed to write JSON to file %s: %s",
                filepath, strerror(errno));
        fclose(fp);
        return (-1);
    }
    fclose(fp);
    return (0);
}

void
mudband_ui_progconf_set_default_band_uuid(const char *band_uuid)
{
    json_t *default_band_uuid;
    
    AN(progconf_jroot);
    default_band_uuid = json_object_get(progconf_jroot, "default_band_uuid");
    if (default_band_uuid != NULL) {
        json_object_del(progconf_jroot, "default_band_uuid");
    }
    json_object_set_new(progconf_jroot, "default_band_uuid",
                        json_string(band_uuid));
    mudband_ui_progconf_write();
}

void
mudband_ui_progconf_set_default_band_uuid_objc(NSString *band_uuid)
{
    
    mudband_ui_progconf_set_default_band_uuid([band_uuid UTF8String]);
}

void
mudband_ui_progconf_delete_default_band_uuid(void)
{
    json_t *default_band_uuid;
    
    AN(progconf_jroot);
    default_band_uuid = json_object_get(progconf_jroot, "default_band_uuid");
    if (default_band_uuid != NULL) {
        json_object_del(progconf_jroot, "default_band_uuid");
    }
    mudband_ui_progconf_write();
}

static int
mudband_ui_progconf_get_default_band_uuid_traversal_dir_callback(struct vtclog *vl,
                                                                 const char *name,
                                                                 void *orig_arg)
{
    struct mudband_ui_traversal_dir_arg *arg =
        (struct mudband_ui_traversal_dir_arg *)orig_arg;
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
    vtc_log(vl, 2, "Found enrollment for the default band UUID: %s/%s", band_enroll_dir, name);
    arg->n_enroll++;
    return (0);
}

const char *
mudband_ui_progconf_get_default_band_uuid(void)
{
    json_t *default_band_uuid;

    AN(progconf_jroot);
    default_band_uuid = json_object_get(progconf_jroot, "default_band_uuid");
    if (default_band_uuid == NULL) {
        struct mudband_ui_traversal_dir_arg dir_arg = { 0, };
        int r;
        
        r = ODR_traversal_dir(progconf_vl, band_enroll_dir,
                              mudband_ui_progconf_get_default_band_uuid_traversal_dir_callback,
                              (void *)&dir_arg);
        if (r != 0) {
            vtc_log(progconf_vl, 0, "BANDEC_00269: ODR_traversal_dir() failed");
            return (NULL);
        }
        if (dir_arg.n_enroll == 0) {
            vtc_log(progconf_vl, 0, "BANDEC_00270: No enrollments found.");
            return (NULL);
        }
        assert(dir_arg.n_enroll > 0);
        mudband_ui_progconf_set_default_band_uuid(dir_arg.b_arg_uuidstr);
        default_band_uuid = json_object_get(progconf_jroot, "default_band_uuid");
        AN(default_band_uuid);
    }
    return (json_string_value(default_band_uuid));
}

void
mudband_ui_progconf_init(void)
{
    
    progconf_vl = vtc_logopen("progconf", NULL);
    AN(progconf_vl);
    progconf_jroot = mudband_ui_progconf_read();
    if (progconf_jroot == NULL) {
        progconf_jroot = json_object();
        AN(progconf_jroot);
        (void)mudband_ui_progconf_write();
    }
    AN(progconf_jroot);
}
