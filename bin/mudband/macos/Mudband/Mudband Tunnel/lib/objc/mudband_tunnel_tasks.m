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

#include <sys/param.h>
#include <arpa/inet.h>

#import <Foundation/Foundation.h>
#import "Mud_band_Tunnel-Swift.h"

#include "callout.h"
#include "odr.h"
#include "odr_pthread.h"
#include "vassert.h"
#include "vtc_log.h"

#include "mudband_tunnel.h"
#include "mudband_tunnel_mqtt.h"
#include "mudband_tunnel_stun_client.h"

extern PacketTunnelProvider *wg_tunnel_provider;

struct wireguard_peer_snapshot *tasks_peer_snapshots;
int tasks_peer_snapshots_count;
static struct callout tasks_conf_nuke_co;
static struct callout tasks_stun_client_co;
static struct callout tasks_conf_fetcher_co;
static struct callout tasks_status_snapshot_co;
static struct callout_block tasks_cb;
static struct vtclog *tasks_vl;
static odr_pthread_t tasks_tp;
static int tasks_aborted;
static int tasks_need_conf_fetcher_trigger;

static void
mudband_tunnel_tasks_stun_client(void *arg)
{

    (void)arg;

    (void)mudband_tunnel_stun_client_test();

    callout_reset(&tasks_cb, &tasks_stun_client_co, CALLOUT_SECTOTICKS(600),
                  mudband_tunnel_tasks_stun_client, NULL);
}

static void
mudband_tunnel_tasks_conf_fetcher(void *arg)
{
    
    if (wg_tunnel_provider == NULL) {
        vtc_log(tasks_vl, 1, "No wg_tunnel_provider set yet.");
        goto done;
    }

    [wg_tunnel_provider mudband_tunnel_confmgr_fetch_mqtt_event];
    
done:
    callout_reset(&tasks_cb, &tasks_conf_fetcher_co,
                  CALLOUT_SECTOTICKS(600), mudband_tunnel_tasks_conf_fetcher, NULL);
}

static void
mudband_tunnel_tasks_status_snapshot(void *arg)
{
    json_t *jroot, *jstats, *jstatus;
    int i;
    const char *default_band_uuid;
    char filepath[PATH_MAX];

    (void)arg;

    wg_band_need_peer_snapahot = 1;    /* trigger a peer snapshot. */
    for (i = 0; i < 3; i++) {
        if (wg_band_need_peer_snapahot == 0)
            break;
        ODR_msleep(1000);
    }
    if (wg_band_need_peer_snapahot == 1) {
        vtc_log(tasks_vl, 1,
                "BANDEC_00884: No peer snapshot performed within"
                " 3 seconds.");
        goto done;
    }
    default_band_uuid = mudband_tunnel_progconf_get_default_band_uuidstr();
    if (default_band_uuid == NULL) {
        vtc_log(tasks_vl, 1, "BANDEC_00885: No default band UUID.");
        goto done;
    }
    jroot = json_object();
    AN(jroot);
    json_object_set_new(jroot, "band_uuid", json_string(default_band_uuid));
    /* peers */
    assert(tasks_peer_snapshots_count >= 0);
    json_t *jpeers = json_array();
    AN(jpeers);
    for (i = 0; i < tasks_peer_snapshots_count; i++) {
        struct in_addr addr;
        json_t *jpeer;
        const char *ip_ptr;
        char ip_str[INET_ADDRSTRLEN];

        jpeer = json_object();
        AN(jpeer);
        /* Convert iface_addr to string */
        addr.s_addr = tasks_peer_snapshots[i].iface_addr;
        ip_ptr = inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));
        AN(ip_ptr);
        json_object_set_new(jpeer, "iface_addr", json_string(ip_str));
        /* Convert endpoint_ip to string */
        addr.s_addr = tasks_peer_snapshots[i].endpoint_ip;
        ip_ptr = inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));
        AN(ip_ptr);
        json_object_set_new(jpeer, "endpoint_ip", json_string(ip_str));
        json_object_set_new(jpeer, "endpoint_port",
            json_integer(tasks_peer_snapshots[i].endpoint_port));
        json_object_set_new(jpeer, "endpoint_t_heartbeated",
            json_integer(tasks_peer_snapshots[i].endpoint_t_heartbeated));
        json_array_append_new(jpeers, jpeer);
    }
    json_object_set_new(jroot, "peers", jpeers);
    /* stats */
    jstats = wireguard_iface_stat_to_json();
    AN(jstats);
    json_object_set_new(jroot, "stats", jstats);
    /* status */
    jstatus = json_object();
    AN(jstatus);
    if (wg_band_mfa_authentication_required) {
        json_object_set_new(jstatus, "mfa_authentication_required",
            json_true());
        if (wg_band_mfa_authentication_url[0] != '\0') {
            json_object_set_new(jstatus, "mfa_authentication_url",
                json_string(wg_band_mfa_authentication_url));
        }
    } else {
        json_object_set_new(jstatus, "mfa_authentication_required",
            json_false());
        json_object_set_new(jstatus, "mfa_authentication_url",
            json_string(""));
    }
    json_object_set_new(jroot, "status", jstatus);

    snprintf(filepath, sizeof(filepath), "%s/%s", band_tunnel_top_dir,
        "status_snapshot.json");
    json_dump_file(jroot, filepath, 0);
    json_decref(jroot);
done:
    callout_reset(&tasks_cb, &tasks_status_snapshot_co,
        CALLOUT_SECTOTICKS(60), mudband_tunnel_tasks_status_snapshot, NULL);

}

static void *
mudband_tunnel_tasks_thread(void *arg)
{
    (void)arg;

    while (!tasks_aborted) {
        if (tasks_need_conf_fetcher_trigger) {
            tasks_need_conf_fetcher_trigger = 0;
            callout_reset(&tasks_cb, &tasks_conf_fetcher_co,
                          CALLOUT_SECTOTICKS(3), mudband_tunnel_tasks_conf_fetcher, NULL);
        }
        COT_ticks(&tasks_cb);
        COT_clock(&tasks_cb);
        mudband_tunnel_mqtt_sync();
        ODR_msleep(500);
    }
    return (NULL);
}

void
mudband_tunnek_tasks_conf_fetcher_trigger(void)
{
    
    tasks_need_conf_fetcher_trigger = 1;
}

static void
mudband_tunnel_tasks_conf_nuke(void *arg)
{
    
    (void)arg;
    
    mudband_tunnel_confmgr_nuke();
    
    callout_reset(&tasks_cb, &tasks_conf_nuke_co, CALLOUT_SECTOTICKS(60),
                  mudband_tunnel_tasks_conf_nuke, NULL);
}

void
mudband_tunnel_tasks_init(void)
{

    COT_init(&tasks_cb);
    callout_init(&tasks_conf_nuke_co, 0);
    callout_init(&tasks_stun_client_co, 0);
    callout_init(&tasks_conf_fetcher_co, 0);
    callout_init(&tasks_status_snapshot_co, 2);
        
    callout_reset(&tasks_cb, &tasks_conf_nuke_co,
                  CALLOUT_SECTOTICKS(60), mudband_tunnel_tasks_conf_nuke,
                  NULL);
    callout_reset(&tasks_cb, &tasks_stun_client_co, CALLOUT_SECTOTICKS(600),
                  mudband_tunnel_tasks_stun_client, NULL);
    callout_reset(&tasks_cb, &tasks_conf_fetcher_co,
                  CALLOUT_SECTOTICKS(600), mudband_tunnel_tasks_conf_fetcher, NULL);
    callout_reset(&tasks_cb, &tasks_status_snapshot_co,
                  CALLOUT_SECTOTICKS(10), mudband_tunnel_tasks_status_snapshot, NULL);

    tasks_vl = vtc_logopen("tasks", mudband_tunnel_log_callback);
    AN(tasks_vl);
    AZ(ODR_pthread_create(&tasks_tp, NULL, mudband_tunnel_tasks_thread,
                          NULL));
    AZ(ODR_pthread_detach(tasks_tp));
}
