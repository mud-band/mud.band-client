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

/* mudband_ui.m */
void    mudband_ui_log(int level, NSString *msg);
void    mudband_ui_set_tapname(NSString *name);
void    mudband_ui_init(NSString *top_dir,
                        NSString *enroll_dir, NSString *admin_dir,
                        NSString *ui_logfile, NSString *tunnel_logfile);
NSMutableArray *
        mudband_ui_create_wireguard_keys(void);

/* mudband_ui_bandadmin.m */
NSString *
        mudband_ui_bandadmin_get(void);
int     mudband_ui_bandadmin_save(NSString *ns_band_uuid, NSString *ns_jwt);

/* mudband_ui_confmgr.m */
NSString *
        mudband_ui_confmgr_get_device_name(void);
NSString *
        mudband_ui_confmgr_get_private_ip(void);

/* mudband_ui_enroll.m */
int     mudband_ui_enroll_get_count(void);
int     mudband_ui_enroll_post(NSString *priv_key, NSString *raw_str);
int     mudband_ui_enroll_load(void);
NSString *
        mudband_ui_enroll_get_band_name(void);
NSString *
        mudband_ui_enroll_get_jwt(void);
NSString *
        mudband_ui_enroll_get_band_uuid(void);
NSMutableArray *
        mudband_ui_enroll_get_band_uuids(void);
void    mudband_ui_enroll_unenroll(NSString *band_uuid);
bool    mudband_ui_enroll_is_public(void);

/* mudband_ui_progconf.m */

void    mudband_ui_progconf_set_default_band_uuid_objc(NSString *band_uuid);
NSString *
        mudband_ui_progconf_get_default_band_uuid_objc(void);
