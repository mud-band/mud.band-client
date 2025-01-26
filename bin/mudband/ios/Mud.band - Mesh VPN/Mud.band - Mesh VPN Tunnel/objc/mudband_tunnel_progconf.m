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

static struct vtclog *progconf_vl;
static json_t *progconf_jroot;
static char *progconf_default_band_uuidstr;
static vuuid_t progconf_default_band_uuid;

static json_t *
mudband_tunnel_progconf_read(void)
{
    json_t *jroot;
    json_error_t jerror;
    char filepath[ODR_BUFSIZ];

    snprintf(filepath, sizeof(filepath), "%s/mudband.conf", band_tunnel_top_dir);
    if (access(filepath, F_OK) == -1) {
        vtc_log(progconf_vl, 0, "BANDEC_00303: File not found: %s",
                filepath);
        return (NULL);
    }
    jroot = json_load_file(filepath, 0, &jerror);
    if (jroot == NULL) {
        vtc_log(progconf_vl, 1,
                "BANDEC_00304: error while reading JSON format:"
                " on line %d: %s", jerror.line, jerror.text);
        return (NULL);
    }
    return (jroot);
}

const char *
mudband_tunnel_progconf_get_default_band_uuidstr(void)
{
    if (progconf_default_band_uuidstr == NULL) {
        json_t *default_band_uuid;
        uint32_t status;
        
        AN(progconf_jroot);
        default_band_uuid = json_object_get(progconf_jroot, "default_band_uuid");
        if (default_band_uuid == NULL)
            return (NULL);
        progconf_default_band_uuidstr = strdup(json_string_value(default_band_uuid));
        AN(progconf_default_band_uuidstr);
        status = vuuid_s_ok;
        VUUID_from_string(progconf_default_band_uuidstr, &progconf_default_band_uuid, &status);
        assert(status == vuuid_s_ok);
    }
    return (progconf_default_band_uuidstr);
}

vuuid_t *
mudband_tunnel_progconf_get_default_band_uuid(void)
{
    
    return (&progconf_default_band_uuid);
}

int
mudband_tunnel_progconf_init(void)
{
    
    progconf_vl = vtc_logopen("progconf", mudband_tunnel_log_callback);
    AN(progconf_vl);
    progconf_jroot = mudband_tunnel_progconf_read();
    if (progconf_jroot == NULL) {
        return (-1);
    }
    AN(progconf_jroot);
    return (0);
}
