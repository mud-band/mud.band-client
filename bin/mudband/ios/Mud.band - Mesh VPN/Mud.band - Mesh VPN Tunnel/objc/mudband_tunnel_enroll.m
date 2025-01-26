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
#include "vassert.h"
#include "vtc_log.h"
#include "vuuid.h"

#include "mudband_tunnel.h"

static struct vtclog *enroll_vl;
static json_t *enroll_jroot;

static json_t *
mudband_tunnel_enroll_band_read(const char *filename)
{
    json_t *jroot, *juuid, *jband_name, *jband_jwt;
    json_error_t jerror;
    char filepath[ODR_BUFSIZ];

    snprintf(filepath, sizeof(filepath), "%s/%s", band_tunnel_enroll_dir, filename);
    if (access(filepath, F_OK) == -1) {
        vtc_log(enroll_vl, 0, "BANDEC_00316: File not found: %s",
            filepath);
        return (NULL);
    }
    jroot = json_load_file(filepath, 0, &jerror);
    if (jroot == NULL) {
        vtc_log(enroll_vl, 1,
            "BANDEC_00317: error while reading JSON format:"
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
mudband_tunnel_enroll_load(void)
{
    const char *default_band_uuid;
    char filename[64];
    
    default_band_uuid = mudband_tunnel_progconf_get_default_band_uuidstr();
    if (default_band_uuid == NULL) {
        vtc_log(enroll_vl, 0, "BANDEC_00318: No default band UUID found.");
        return (-1);
    }
    vtc_log(enroll_vl, 2, "Selected the enrollment for band uuid %s",
            default_band_uuid);
    snprintf(filename, sizeof(filename), "band_%s.json",
             default_band_uuid);
    if (enroll_jroot != NULL)
        json_decref(enroll_jroot);
    enroll_jroot = mudband_tunnel_enroll_band_read(filename);
    AN(enroll_jroot);
    return (0);
}

NSString *
mudband_tunnel_enroll_get_jwt(void)
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

const char *
mudband_tunnel_enroll_get_private_key(void)
{
    json_t *wireguard_privkey;

    AN(enroll_jroot);
    wireguard_privkey = json_object_get(enroll_jroot, "wireguard_privkey");
    AN(wireguard_privkey);
    assert(json_is_string(wireguard_privkey));
    assert(json_string_length(wireguard_privkey) > 0);
    return (json_string_value(wireguard_privkey));
}

const char *
mudband_tunnel_enroll_get_uuidstr(void)
{
    
    return (mudband_tunnel_progconf_get_default_band_uuidstr());
}

int
mudband_tunnel_enroll_init(void)
{
    int r;
    
    enroll_vl = vtc_logopen("enroll", mudband_tunnel_log_callback);
    AN(enroll_vl);
    r = mudband_tunnel_enroll_load();
    if (r != 0)
        return (-1);
    return (0);
}
