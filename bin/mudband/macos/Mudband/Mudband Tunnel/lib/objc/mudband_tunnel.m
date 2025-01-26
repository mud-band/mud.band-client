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

#include <os/log.h>

#include "callout.h"
#include "odr.h"
#include "vassert.h"
#include "vtc_log.h"

#include "mudband_tunnel.h"
#include "mudband_tunnel_mqtt.h"
#include "mudband_tunnel_stun_client.h"
#include "wireguard.h"

static struct vtclog *band_tunnel_vl;
char *band_tunnel_enroll_dir;
char *band_tunnel_top_dir;

void
VAS_Fail(const char *func, const char *file, int line, const char *cond,
        int xxx)
{
    char buf[ODR_BUFSIZ];
    
    snprintf(buf, sizeof(buf), "Critical! assert fail: %s %s:%d %s %d",
             func, file, line, cond, xxx);
    os_log_error(OS_LOG_DEFAULT, "MUDBAND ==> %{public}s", buf);
    abort();
}

static const char * const mudband_tunnel_log_lead[] = {
    "[ERROR]",
    "[WARN]",
    "[INFO]",
    "[DEBUG]",
    "[TRACE]"
};

int
mudband_tunnel_log_callback(const char *id, int lvl, double t_elapsed, const char *msg)
{
    char nowstr[ODR_TIME_FORMAT_SIZE], line[1024];
    
    ODR_TimeFormat(nowstr, "%a, %d %b %Y %T GMT", ODR_real());
    snprintf(line, sizeof(line), "%s [%f] %-4s %s %s", nowstr, t_elapsed,
        id, mudband_tunnel_log_lead[lvl], msg);
    switch (lvl) {
        case 0:
        case 1:
            os_log_error(OS_LOG_DEFAULT, "%{public}s", line);
            break;
        case 2:
            os_log(OS_LOG_DEFAULT, "%{public}s", line);
            break;
        case 3:
        default:
            os_log_debug(OS_LOG_DEFAULT, "%{public}s", line);
            break;
    }
    return (1);
}

void
mudband_tunnel_log(int level, NSString *msg)
{
    const char *msgptr = [msg UTF8String];
    
    vtc_log(band_tunnel_vl, level, msgptr);
}

int
mudband_tunnel_init(NSString *top_dir, NSString *enroll_dir)
{
    int r;
    
    ODR_libinit();
    vtc_loginit();
    band_tunnel_top_dir = strdup([top_dir UTF8String]);
    AN(band_tunnel_top_dir);
    band_tunnel_enroll_dir = strdup([enroll_dir UTF8String]);
    AN(band_tunnel_enroll_dir);
    band_tunnel_vl = vtc_logopen("tunnel", mudband_tunnel_log_callback);
    AN(band_tunnel_vl);
    r = mudband_tunnel_progconf_init();
    if (r == -1) {
        vtc_log(band_tunnel_vl, 0,
                "BANDEC_00364: mudband_tunnel_progconf_init() failed.");
        return (-1);
    }
    r = mudband_tunnel_enroll_init();
    if (r == -1) {
        vtc_log(band_tunnel_vl, 0,
                "BANDEC_00365: mudband_tunnel_enroll_init() failed.");
        return (-1);
    }
    r = mudband_tunnel_confmgr_init();
    if (r == -1) {
        vtc_log(band_tunnel_vl, 0,
                "BANDEC_00366: mudband_tunnel_confmgr_init() failed.");
        return (-1);
    }
    r = mudband_tunnel_connmgr_init();
    if (r == -1) {
        vtc_log(band_tunnel_vl, 0,
                "BANDEC_00367: mudband_tunnel_connmgr_init() failed.");
        return (-1);
    }
    r = mudband_tunnel_stun_client_init();
    assert(r == 0);
    r = mudband_tunnel_mqtt_init();
    if (r == -1) {
        vtc_log(band_tunnel_vl, 0,
                "BANDEC_00368: mudband_tunnel_mqtt_init() failed.");
        return (-1);
    }
    mudband_tunnel_tasks_init();
    mudband_tunnel_mqtt_subscribe();

    vtc_log(band_tunnel_vl, 2, "Initialized the mudband tunnel.");
    return (0);
}

