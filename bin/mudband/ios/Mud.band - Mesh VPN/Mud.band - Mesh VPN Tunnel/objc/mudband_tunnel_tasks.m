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
#import "Mud_band___Mesh_VPN_Tunnel-Swift.h"

#include "callout.h"
#include "odr.h"
#include "odr_pthread.h"
#include "vassert.h"
#include "vtc_log.h"

#include "mudband_tunnel.h"
#include "mudband_tunnel_mqtt.h"
#include "mudband_tunnel_stun_client.h"

extern PacketTunnelProvider *wg_tunnel_provider;

static struct callout tasks_conf_nuke_co;
static struct callout tasks_stun_client_co;
static struct callout tasks_conf_fetcher_co;
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
        
    callout_reset(&tasks_cb, &tasks_conf_nuke_co,
                  CALLOUT_SECTOTICKS(60), mudband_tunnel_tasks_conf_nuke,
                  NULL);
    callout_reset(&tasks_cb, &tasks_stun_client_co, CALLOUT_SECTOTICKS(600),
                  mudband_tunnel_tasks_stun_client, NULL);
    callout_reset(&tasks_cb, &tasks_conf_fetcher_co,
                  CALLOUT_SECTOTICKS(600), mudband_tunnel_tasks_conf_fetcher, NULL);

    tasks_vl = vtc_logopen("tasks", mudband_tunnel_log_callback);
    AN(tasks_vl);
    AZ(ODR_pthread_create(&tasks_tp, NULL, mudband_tunnel_tasks_thread,
                          NULL));
    AZ(ODR_pthread_detach(tasks_tp));
}
