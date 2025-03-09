//
//  mudband_tunnel.h
//  Mudband Tunnel
//
//  Created by Weongyo Jeong on 12/12/24.
//

#ifndef MUDBAND_TUNNEL_H
#define MUDBAND_TUNNEL_H

#include <stdbool.h>

#include "jansson.h"
#include "vassert.h"
#include "vqueue.h"
#include "vuuid.h"

/* Assert zero return value */
#define AZ(foo)    do { assert((foo) == 0); } while (0)
#define AN(foo)    do { assert((foo) != 0); } while (0)

#define    WIREGUARD_IFACE_PEER_ENDPOINTS_MAX    16
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
    
    bool otp_enabled;
    uint64_t otp_sender;
    uint64_t otp_receiver[3];
};

/* mudband_tunnel.m */
extern char *band_tunnel_enroll_dir;
extern char *band_tunnel_top_dir;
extern int wg_band_need_peer_snapahot;
int     mudband_tunnel_log_callback(const char *id, int lvl, double t_elapsed,
                                    const char *msg);

/* mudband_tunnel_confmgr.m */
struct mudband_tunnel_bandconf {
    json_t          *jroot;
    int             busy;
    time_t          t_last;
    VTAILQ_ENTRY(mudband_tunnel_bandconf) list;
};
int     mudband_tunnel_confmgr_init(void);
int     mudband_tunnel_confmgr_get(struct mudband_tunnel_bandconf **cfp);
void    mudband_tunnel_confmgr_rel(struct mudband_tunnel_bandconf **cfp);
int     mudband_tunnel_confmgr_get_interface_listen_port(json_t *jroot);
const char *
        mudband_tunnel_confmgr_get_interface_private_ip_by_obj(json_t *jroot);
int     mudband_tunnel_confmgr_get_peer_size(json_t *jroot);
int     mudband_tunnel_confmgr_fill_iface_peer(json_t *jroot, struct wireguard_iface_peer *peer,
                                               int idx);
const char *
        mudband_tunnel_confmgr_get_interface_device_uuid(json_t *jroot);
struct wireguard_acl *
        mudband_tunnel_confmgr_acl_build(json_t *jroot);

/* mudband_tunnel_connmgr.m */
int     mudband_tunnel_connmgr_init(void);
void    mudband_tunnel_confmgr_nuke(void);
int     mudband_tunnel_connmgr_listen_port(void);
int     mudband_tunnel_connmgr_listen_fd(void);

/* mudband_tunnel_enroll.m */
int     mudband_tunnel_enroll_init(void);
const char *
        mudband_tunnel_enroll_get_uuidstr(void);
const char *
        mudband_tunnel_enroll_get_private_key(void);

/* mudband_tunnel_progconf.m */
int     mudband_tunnel_progconf_init(void);
const char *
        mudband_tunnel_progconf_get_default_band_uuidstr(void);
vuuid_t *
        mudband_tunnel_progconf_get_default_band_uuid(void);

/* mudband_tunnel_tasks.m */
struct wireguard_peer_snapshot {
    uint32_t    iface_addr;
    uint32_t    endpoint_ip;
    uint16_t    endpoint_port;
    time_t        endpoint_t_heartbeated;
};
extern struct wireguard_peer_snapshot *tasks_peer_snapshots;
extern int tasks_peer_snapshots_count;
void    mudband_tunnel_tasks_init(void);
void    mudband_tunnek_tasks_conf_fetcher_trigger(void);

/* mudband_tunnel_wireguard.m */
extern int wg_band_mfa_authentication_required;
extern char wg_band_mfa_authentication_url[512];
json_t *wireguard_iface_stat_to_json(void);

#endif /* MUDBAND_TUNNEL_H */
