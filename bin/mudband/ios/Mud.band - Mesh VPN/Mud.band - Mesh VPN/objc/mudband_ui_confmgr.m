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

static struct vtclog *confmgr_vl;

static json_t *
mudband_ui_confmgr_read(const char *filename)
{
    json_t *jroot;
    json_error_t jerror;
    char filepath[ODR_BUFSIZ];

    snprintf(filepath, sizeof(filepath), "%s/%s", band_enroll_dir, filename);
    if (access(filepath, F_OK) == -1) {
        vtc_log(confmgr_vl, 0, "BANDEC_00800: File not found: %s",
            filepath);
        return (NULL);
    }
    jroot = json_load_file(filepath, 0, &jerror);
    if (jroot == NULL) {
        vtc_log(confmgr_vl, 1,
            "BANDEC_00801: error while reading JSON format:"
            " on line %d: %s", jerror.line, jerror.text);
        return (NULL);
    }
    assert(json_is_object(jroot));
    return (jroot);
}

static json_t *
mudband_ui_confmgr_load(const char *band_uuid)
{
    json_t *jroot;
    char filename[64];

    snprintf(filename, sizeof(filename), "conf_%s.json",
             band_uuid);
    jroot = mudband_ui_confmgr_read(filename);
    if (jroot == NULL)
        return (NULL);
    return (jroot);
}

static json_t *
mudband_ui_confmgr_default_load(void)
{
    json_t *jroot;
    const char *band_uuid;

    band_uuid = mudband_ui_progconf_get_default_band_uuid();
    if (band_uuid == NULL)
        return (NULL);
    jroot = mudband_ui_confmgr_load(band_uuid);
    if (jroot == NULL)
        return (NULL);
    return (jroot);
}

NSString *
mudband_ui_confmgr_get_device_name(void)
{
    json_t *jroot, *jinterface, *jname;
    NSString *str;

    jroot = mudband_ui_confmgr_default_load();
    if (jroot == NULL)
        return [NSString stringWithUTF8String:""];
    jinterface = json_object_get(jroot, "interface");
    AN(jinterface);
    assert(json_is_object(jinterface));
    jname = json_object_get(jinterface, "name");
    AN(jname);
    assert(json_is_string(jname));
    str = [NSString stringWithUTF8String:json_string_value(jname)];
    json_decref(jroot);
    return (str);
}

NSString *
mudband_ui_confmgr_get_private_ip(void)
{
    json_t *jroot, *jinterface, *jprivate_ip;
    NSString *str;

    jroot = mudband_ui_confmgr_default_load();
    if (jroot == NULL)
        return [NSString stringWithUTF8String:""];
    jinterface = json_object_get(jroot, "interface");
    AN(jinterface);
    assert(json_is_object(jinterface));
    jprivate_ip = json_object_get(jinterface, "private_ip");
    AN(jprivate_ip);
    assert(json_is_string(jprivate_ip));
    str = [NSString stringWithUTF8String:json_string_value(jprivate_ip)];
    json_decref(jroot);
    return (str);
}

void
mudband_ui_confmgr_init(void)
{
    confmgr_vl = vtc_logopen("confmgr", NULL);
    AN(confmgr_vl);
}
