/*
 * Copyright (c) 2024 Weongyo Jeong (weongyo@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of "Floorsense Ltd", "Agile Workspace Ltd" nor
 *    the names of its contributors may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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

#ifndef MUD_BAND_MUDBAND_H
#define MUD_BAND_MUDBAND_H

#include <stdbool.h>

#include "jansson.h"
#include "vqueue.h"
#include "vuuid.h"

/* mudband.c */
extern char band_root_dir[256];
extern char band_enroll_dir[256];
extern char band_admin_dir[256];
extern int band_need_fetch_config;
extern int band_need_iface_sync;
int     mudband_log_printf(const char *id, int lvl, double t_elapsed, const char *msg);
void    mudband_tunnel_iface_write(uint8_t *buf, size_t buflen);
uint8_t *
        mudband_tunnel_proxy_prepend_pkthdr(uint8_t *buf, size_t *buflen,
                                            uint32_t src_addr, uint32_t dst_addr);

/* mudband_bandadmin.c */
int     MBA_save(const char *band_uuid, const char *jwt);
json_t *MBA_get(void);
int     MBA_init(void);

/* mudband_confmgr.c */
struct cnf {
    json_t          *jroot;
    int             busy;
    time_t          t_last;
    VTAILQ_ENTRY(cnf) list;
};
int     CNF_init(void);
int     CNF_get(struct cnf **cfp);
void    CNF_rel(struct cnf **cfp);
const char *
        CNF_get_etag(json_t *jroot);
int     CNF_parse_response(const char *etag, const char *body);
void    CNF_nuke(void);
json_t *CNF_getifaddrs(void);
int     CNF_get_interface_listen_port(json_t *jroot);
const char *
        CNF_get_interface_private_ip(json_t *jroot);
const char *
        CNF_get_interface_private_mask(json_t *jroot);
int     CNF_get_interface_mtu(json_t *jroot);
int     CNF_get_peer_size(json_t *jroot);
struct wireguard_iface_peer;
int     CNF_fill_iface_peer(json_t *jroot, struct wireguard_iface_peer *peer, int idx);
int     CNF_load(void);
struct wireguard_acl *
        CNF_acl_build(json_t *jroot);
const char *
	    CNF_get_interface_device_uuid(json_t *jroot);
const char *
        CNF_get_interface_name(json_t *jroot);

/* mudband_connmgr.c */
int     MCM_init(void);
int     MCM_listen_port(void);
int     MCM_listen_fd(void);

/* mudband_enroll.c */
extern json_t *mbe_jroot;
int     MBE_init(void);
int     MBE_parse_enroll_response(const char *private_key, const char *body);
int     MBE_check_and_read();
int     MBE_get_band_name(char *buf, size_t bufmax);
const char *
        MBE_get_jwt(void);
const char *
        MBE_get_private_key(void);
const vuuid_t *
        MBE_get_uuid(void);
int     MBE_parse_unenroll_response(const char *body);
int     MBE_get_band_name_by_uuid(const char *uuid, char *buf, size_t bufmax);

/* mudband_progconf.c */
void	MPC_set_default_band_uuid(const char *band_uuid);
void	MPC_delete_default_band_uuid(void);
const char *
MPC_get_default_band_uuid(void);
void	MPC_init(void);
int     MBE_is_public(void);

/* mudband_tasks.c */
int     MBT_init(void);
void    MBT_conf_fetcher_trigger(void);

#endif //MUD_BAND_MUDBAND_H
