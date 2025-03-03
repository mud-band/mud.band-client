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

#ifndef mudband_ui_h
#define mudband_ui_h

struct mudband_ui_traversal_dir_arg {
    int     n_enroll;
    int     b_arg_found;
    char    b_arg_uuidstr[64];
    void    *arg;
};

/* mudband_ui.m */
extern const char *band_enroll_dir;
extern const char *band_top_dir;
extern const char *band_admin_dir;

/* mudband_ui_bandadmin.m */
int     mudband_ui_bandadmin_init(void);

/* mudband_ui_confmgr.m */
void    mudband_ui_confmgr_init(void);

/* mudband_ui_enroll.m */
void    mudband_ui_enroll_init(void);

/* mudband_ui_progconf.m */
void    mudband_ui_progconf_init(void);
void    mudband_ui_progconf_set_default_band_uuid(const char *band_uuid);
const char *
        mudband_ui_progconf_get_default_band_uuid(void);
void    mudband_ui_progconf_delete_default_band_uuid(void);

#endif /* mudband_ui_h */
