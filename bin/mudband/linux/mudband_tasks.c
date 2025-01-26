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

#include "mudband.h"
#include "mudband_mqtt.h"
#include "mudband_stun_client.h"

#include "callout.h"
#include "odr.h"
#include "odr_pthread.h"
#include "vtc_log.h"

static struct vtclog *mbt_vl;
static struct callout_block mbt_cb;
static struct callout mbt_stun_client_co;
static struct callout mbt_conf_fetcher_co;
static struct callout mbt_conf_nuke_co;
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

	callout_reset(&mbt_cb, &mbt_conf_nuke_co,
	    CALLOUT_SECTOTICKS(60), mbt_conf_nuke, NULL);
	callout_reset(&mbt_cb, &mbt_conf_fetcher_co,
	    CALLOUT_SECTOTICKS(600), mbt_conf_fetcher, NULL);
	callout_reset(&mbt_cb, &mbt_stun_client_co,
	    CALLOUT_SECTOTICKS(600), mbt_stun_client, NULL);

	AZ(ODR_pthread_create(&mbt_tp, NULL, mbt_thread, NULL));
	AZ(ODR_pthread_detach(mbt_tp));
	return (0);
}
