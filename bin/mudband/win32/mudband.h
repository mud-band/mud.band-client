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

#ifndef _MUDWG_H_
#define _MUDWG_H_

#include "jansson.h"
#include "vqueue.h"
#include "vuuid.h"

#include "crypto.h"
#include "wireguard.h"

/* mudband.c */
#define	WIREGUARD_IFACE_PEER_ENDPOINTS_MAX	16
struct wireguard_iface_peer {
	const char *public_key;
	/*
	 * Optional pre-shared key (32 bytes)
	 * make sure this is NULL if not to be used
	 */
	const uint8_t *preshared_key;
	/*
	 * tai64n of largest timestamp we have seen during handshake to
	 * avoid replays
	 */
	uint8_t greatest_timestamp[12];

	uint32_t iface_addr;

	/*
	 * Allowed ip/netmask (can add additional later but at least one
	 * is required)
	 */
	uint32_t allowed_ip;
	uint32_t allowed_mask;

	/* End-point details (may be blank) */
	struct {
		bool is_proxy;
		uint32_t ip;
		uint16_t port;
	} endpoints[WIREGUARD_IFACE_PEER_ENDPOINTS_MAX];
	uint8_t n_endpoints;
	uint16_t keep_alive;
};
extern const char *band_b_arg;
extern char *band_confdir_enroll;
extern char *band_confdir_root;
extern int band_need_iface_sync;

/* mudband_acl.c */
int	ACL_init(void);
int	ACL_cmd(const char *acl_add_arg, const char *acl_priority_arg,
	    unsigned acl_list_flag, const char *acl_del_arg,
	    const char *acl_default_policy_arg);

/* mudband_confmgr.c */
struct cnf {
	json_t		*jroot;
	int		busy;
	time_t		t_last;
	VTAILQ_ENTRY(cnf) list;
};
int	CNF_init(void);
void	CNF_fini(void);
void	CNF_nuke(void);
int	CNF_check_and_read(void);
int	CNF_get(struct cnf **cfp);
void	CNF_rel(struct cnf **cfp);
int	CNF_fill_iface_peer(json_t *, struct wireguard_iface_peer *peer,
	    int idx);
int	CNF_get_interface_mtu(json_t *);
int	CNF_get_interface_listen_port(json_t *);
const char *
	CNF_get_interface_private_ip(json_t *);
const char *
	CNF_get_interface_private_mask(json_t *);
int	CNF_get_interface_listen_fd(void);
int	CNF_fetch(const char *fetch_type);
int	CNF_get_peer_size(json_t *);
struct wireguard_acl *
	CNF_acl_build(json_t *jroot);
const char *
	CNF_get_interface_device_uuid(json_t *jroot);

/* mudband_connmgr.c */
int	MCM_init(void);
const char *
	MCM_listen_portstr(void);
int	MCM_listen_port(void);
int	MCM_listen_fd(void);

/* mudband_enroll.c */
extern json_t *mbe_jroot;
int	MBE_init(void);
void	MBE_fini(void);
int	MBE_enroll(const char *token, const char *name, const char *secret);
int	MBE_check_and_read(void);
const char *
	MBE_get_private_key(void);
const char *
	MBE_get_uuidstr(void);

const vuuid_t *
	MBE_get_uuid(void);
int	MBE_list(void);

/* mudband_progconf.c */
void	MPC_set_default_band_uuid(const char *band_uuid);
void	MPC_delete_default_band_uuid(void);
const char *
	MPC_get_default_band_uuid(void);
void	MPC_init(void);

/* mudband_tasks.c */
int	MBT_init(void);
void	MBT_fini(void);
void	MBT_conf_fetcher_trigger(void);

/* mudband_webcli.c */
int	MWC_init(void);
int	MWC_get(void);

#endif
