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
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "jansson.h"
#include "mudband.h"
#include "mudband_mqtt.h"
#include "mudband_stun_client.h"

#include "callout.h"
#include "odr.h"
#include "odr_pthread.h"
#include "vtc_log.h"

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
	int r;

	(void)arg;

	r = CNF_fetch("when_it_gots_a_event");
	if (r < 0 ) {
		vtc_log(mbt_vl, 1, "Failed to fetch the configuration. (r %d)",
		    r);
		goto done;
	}
	if (r == 1) {
		vtc_log(mbt_vl, 2, "Skip to check and read the config");
		goto done;
	}
	r = CNF_check_and_read();
	switch (r) {
	case -3:	/* nat type changed */
	case -4:	/* mapped address changed */
	case -5:	/* no peers exist */
		break;
	case 0:
		band_need_iface_sync = 1;
		break;
	default:
		vtc_log(mbt_vl, 2,
		    "BANDEC_00138: Failed to read the config from the disk.");
		break;
	}
done:
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

	band_need_peer_snapahot = 1;	/* trigger a peer snapshot. */
	for (i = 0; i < 3; i++) {
		if (band_need_peer_snapahot == 0)
			break;
		ODR_msleep(1000);
	}
	if (band_need_peer_snapahot == 1) {
		vtc_log(mbt_vl, 1,
		    "BANDEC_XXXXX: No peer snapshot performed within"
		    " 3 seconds.");
		return;
	}
	default_band_uuid = MPC_get_default_band_uuid();
	if (default_band_uuid == NULL) {
		vtc_log(mbt_vl, 1, "BANDEC_XXXXX: No default band UUID.");
		return;
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
	}
	json_object_set_new(jroot, "status", jstatus);

	snprintf(filepath, sizeof(filepath), "%s/%s", band_confdir_root,
	    "status_snapshot.json");
	json_dump_file(jroot, filepath, 0);
	json_decref(jroot);

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

void
MBT_fini(void)
{

	mbt_aborted = 1;
	ODR_pthread_free(mbt_tp);
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
	    CALLOUT_SECTOTICKS(600), mbt_stun_client, NULL);
	if (status_snapshot_flag) {
		callout_reset(&mbt_cb, &mbt_status_snapshot_co,
		    CALLOUT_SECTOTICKS(10), mbt_status_snapshot, NULL);
	}

	AZ(ODR_pthread_create(&mbt_tp, NULL, mbt_thread, NULL));
	AZ(ODR_pthread_detach(mbt_tp));
	return (0);
}
