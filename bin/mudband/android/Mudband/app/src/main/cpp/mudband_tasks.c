/*-
 * Copyright (c) 2014-2022 Weongyo Jeong <weongyo@gmail.com>
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

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "callout.h"
#include "odr.h"
#include "odr_pthread.h"
#include "vassert.h"
#include "vtc_log.h"

#include "mudband.h"
#include "mudband_mqtt.h"
#include "mudband_stun_client.h"
#include "mudband_wireguard.h"

struct wireguard_peer_snapshot *mbt_peer_snapshots;
int mbt_peer_snapshots_count;
static struct vtclog *mbt_vl;
static struct callout_block mbt_cb;
static struct callout mbt_stun_client_co;
static struct callout mbt_conf_fetcher_co;
static struct callout mbt_conf_nuke_co;
static struct callout mbt_status_snapshot_co;
static odr_pthread_t mbt_tp;
static int mbt_need_conf_fetcher_trigger;
static int mbt_aborted;

static void
mbt_stun_client(void *arg)
{

    (void)arg;

    STUNC_test();

    callout_reset(&mbt_cb, &mbt_stun_client_co, CALLOUT_SECTOTICKS(600),
                  mbt_stun_client, NULL);
}

static void
mbt_conf_fetcher(void *arg)
{

    (void)arg;

    band_need_fetch_config = 1;

    callout_reset(&mbt_cb, &mbt_conf_fetcher_co,
                  CALLOUT_SECTOTICKS(600), mbt_conf_fetcher, NULL);
}

static void
mbt_conf_nuke(void *arg)
{

    (void)arg;

    CNF_nuke();

    callout_reset(&mbt_cb, &mbt_conf_nuke_co,
                  CALLOUT_SECTOTICKS(60), mbt_conf_nuke, NULL);
}

void
MBT_conf_fetcher_trigger(void)
{

    /*
     * signal to fetch the configuration.  Don't call callout_reset()
     * directly because we're on the multi-threads.
     */
    mbt_need_conf_fetcher_trigger = 1;
    vtc_log(mbt_vl, 2, "Trigger the conf fetcher.");
}

static void
mbt_status_snapshot(void *arg)
{
    json_t *jroot, *jstats, *jstatus;
    int i;
    const char *default_band_uuid;
    char filepath[PATH_MAX];

    (void)arg;

    band_need_peer_snapshot = 1;	/* trigger a peer snapshot. */
    for (i = 0; i < 3; i++) {
        if (band_need_peer_snapshot == 0)
            break;
        ODR_msleep(1000);
    }
    if (band_need_peer_snapshot == 1) {
        vtc_log(mbt_vl, 1,
                "BANDEC_00874: No peer snapshot performed within"
                " 3 seconds.");
        goto done;
    }
    default_band_uuid = MPC_get_default_band_uuid();
    if (default_band_uuid == NULL) {
        vtc_log(mbt_vl, 1, "BANDEC_00875: No default band UUID.");
        goto done;
    }
    jroot = json_object();
    AN(jroot);
    json_object_set_new(jroot, "band_uuid", json_string(default_band_uuid));
    /* peers */
    assert(mbt_peer_snapshots_count >= 0);
    json_t *jpeers = json_array();
    AN(jpeers);
    for (i = 0; i < mbt_peer_snapshots_count; i++) {
        struct in_addr addr;
        json_t *jpeer;
        const char *ip_ptr;
        char ip_str[INET_ADDRSTRLEN];

        jpeer = json_object();
        AN(jpeer);
        /* Convert iface_addr to string */
        addr.s_addr = mbt_peer_snapshots[i].iface_addr;
        ip_ptr = inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));
        AN(ip_ptr);
        json_object_set_new(jpeer, "iface_addr", json_string(ip_str));
        /* Convert endpoint_ip to string */
        addr.s_addr = mbt_peer_snapshots[i].endpoint_ip;
        ip_ptr = inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));
        AN(ip_ptr);
        json_object_set_new(jpeer, "endpoint_ip", json_string(ip_str));
        json_object_set_new(jpeer, "endpoint_port",
                            json_integer(mbt_peer_snapshots[i].endpoint_port));
        json_object_set_new(jpeer, "endpoint_t_heartbeated",
                            json_integer(mbt_peer_snapshots[i].endpoint_t_heartbeated));
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
    if (band_mfa_authentication_required) {
        json_object_set_new(jstatus, "mfa_authentication_required",
                            json_true());
        if (band_mfa_authentication_url[0] != '\0') {
            json_object_set_new(jstatus, "mfa_authentication_url",
                                json_string(band_mfa_authentication_url));
        }
    } else {
        json_object_set_new(jstatus, "mfa_authentication_required",
                            json_false());
        json_object_set_new(jstatus, "mfa_authentication_url",
                            json_string(""));
    }
    json_object_set_new(jroot, "status", jstatus);

    snprintf(filepath, sizeof(filepath), "%s/%s", band_root_dir,
             "status_snapshot.json");
    json_dump_file(jroot, filepath, 0);
    json_decref(jroot);
done:
    callout_reset(&mbt_cb, &mbt_status_snapshot_co,
                  CALLOUT_SECTOTICKS(60), mbt_status_snapshot, NULL);
}

static void *
mbt_thread(void *arg)
{

    (void)arg;

    while (!mbt_aborted) {
        if (mbt_need_conf_fetcher_trigger) {
            mbt_need_conf_fetcher_trigger = 0;
            callout_reset(&mbt_cb, &mbt_conf_fetcher_co,
                          CALLOUT_SECTOTICKS(3), mbt_conf_fetcher, NULL);
        }
        COT_ticks(&mbt_cb);
        COT_clock(&mbt_cb);
        MQTT_sync();
        ODR_msleep(500);
    }
    return (NULL);
}

int
MBT_init(void)
{

    mbt_vl = vtc_logopen("tasks", mudband_log_printf);
    AN(mbt_vl);

    COT_init(&mbt_cb);
    callout_init(&mbt_conf_nuke_co, 0);
    callout_init(&mbt_conf_fetcher_co, 0);
    callout_init(&mbt_stun_client_co, 1);
    callout_init(&mbt_status_snapshot_co, 2);

    callout_reset(&mbt_cb, &mbt_conf_nuke_co,
                  CALLOUT_SECTOTICKS(60), mbt_conf_nuke, NULL);
    callout_reset(&mbt_cb, &mbt_conf_fetcher_co,
                  CALLOUT_SECTOTICKS(600), mbt_conf_fetcher, NULL);
    callout_reset(&mbt_cb, &mbt_stun_client_co,
                  CALLOUT_SECTOTICKS(0), mbt_stun_client, NULL);
    callout_reset(&mbt_cb, &mbt_status_snapshot_co,
                  CALLOUT_SECTOTICKS(10), mbt_status_snapshot, NULL);

    AZ(ODR_pthread_create(&mbt_tp, NULL, mbt_thread, NULL));
    AZ(ODR_pthread_detach(mbt_tp));
    return (0);
}
