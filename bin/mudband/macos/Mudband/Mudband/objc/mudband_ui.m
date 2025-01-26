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

static struct vtclog *band_vl;
static const char *band_ui_logfile;
static const char *band_tunnel_logfile;
const char *band_enroll_dir;
const char *band_top_dir;

void
VAS_Fail(const char *func, const char *file, int line, const char *cond,
        int xxx)
{

    fprintf(stdout, "Critical! assert fail: %s %s:%d %s %d\n", func, file,
        line, cond, xxx);
    abort();
}

void
mudband_ui_log(int level, NSString *msg)
{
    const char *msgptr = [msg UTF8String];
    
    vtc_log(band_vl, level, msgptr);
}

void
mudband_ui_set_tapname(NSString *name)
{

    NSLog(@"-> set_tap_name: %@", name);
}

NSMutableArray *
mudband_ui_create_wireguard_keys(void)
{
    NSMutableArray *mutableArray = [[NSMutableArray alloc] init];
    size_t wg_pubkeystrlen, wg_privkeystrlen;
    uint8_t wg_privkey[WIREGUARD_PRIVATE_KEY_LEN];
    uint8_t wg_pubkey[WIREGUARD_PUBLIC_KEY_LEN];
    char wg_pubkeystr[WIREGUARD_PUBLIC_KEY_LEN * 2 + 1 /* XXX */];
    char wg_privkeystr[WIREGUARD_PRIVATE_KEY_LEN * 2 + 1 /* XXX */];
    bool success;
    
    /* generate wireguard key pair */
    wireguard_generate_private_key(wg_privkey);
    wireguard_generate_public_key(wg_pubkey, wg_privkey);
    wg_pubkeystrlen = sizeof(wg_pubkeystr);
    success = wireguard_base64_encode(wg_pubkey, sizeof(wg_pubkey),
        wg_pubkeystr, &wg_pubkeystrlen);
    if (!success) {
        vtc_log(band_vl, 0, "BANDEC_00408: wireguard_base64_encode() failed.");
        return (NULL);
    }
    wg_privkeystrlen = sizeof(wg_privkeystr);
    success = wireguard_base64_encode(wg_privkey, sizeof(wg_privkey),
        wg_privkeystr, &wg_privkeystrlen);
    if (!success) {
        vtc_log(band_vl, 0, "BANDEC_00409: wireguard_base64_encode() failed.");
        return (NULL);
    }
    [mutableArray addObject:[NSString stringWithUTF8String:wg_pubkeystr]];
    [mutableArray addObject:[NSString stringWithUTF8String:wg_privkeystr]];
    return mutableArray;
}

void
mudband_ui_init(NSString *top_dir, NSString *enroll_dir, NSString *ui_logfile,
                NSString *tunnel_logfile)
{

    ODR_libinit();
    vtc_loginit();
    band_ui_logfile = strdup([ui_logfile UTF8String]);
    AN(band_ui_logfile);
    band_tunnel_logfile = strdup([tunnel_logfile UTF8String]);
    AN(band_tunnel_logfile);
    band_top_dir = strdup([top_dir UTF8String]);
    AN(band_top_dir);
    band_enroll_dir = strdup([enroll_dir UTF8String]);
    AN(band_enroll_dir);
    band_vl = vtc_logopen("band", NULL);
    AN(band_vl);
    mudband_ui_enroll_init();
    mudband_ui_progconf_init();
}
