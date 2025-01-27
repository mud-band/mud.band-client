/*
 * Copyright (c) 2024 Weongyo Jeong (weongyo@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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

#include <windows.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "mudband.h"

#include "jansson.h"
#include "odr.h"
#include "vassert.h"
#include "vhttps.h"
#include "vopt.h"
#include "vtc_log.h"
#include "vuuid.h"

static struct vtclog *mwc_vl;

int
MWC_get(void)
{
	struct vhttps_req req;
	json_error_t jerror;
	json_t *jroot, *jband_jwt, *jstatus, *jmsg, *jurl;
	size_t resp_bodylen;
	int hdrslen, r;
	char hdrs[ODR_BUFSIZ];
	char resp_body[4096];

	r = MBE_check_and_read();
	if (r == -1) {
		vtc_log(mwc_vl, 0, "BANDEC_00458: Enrollment check failed.");
		return (1);
	}
	AN(mbe_jroot);
	jband_jwt = json_object_get(mbe_jroot, "jwt");
	AN(jband_jwt);

	ODR_bzero(&req, sizeof(req));

	req.f_need_resp_status = 1;
	req.vl = mwc_vl;
	req.server = "www.mud.band:443";
	req.domain = "www.mud.band";
	req.url = "/webcli/signin";
	hdrslen = ODR_snprintf(hdrs, sizeof(hdrs),
	    "Authorization: %s\r\n"
	    "Content-Type: application/json\r\n"
	    "Host: www.mud.band\r\n", json_string_value(jband_jwt));
	assert(hdrslen < sizeof(hdrs));
	req.hdrs = hdrs;
	resp_bodylen = sizeof(resp_body);
	r = VHTTPS_get(&req, resp_body, &resp_bodylen);
	if (r == -1) {
		vtc_log(mwc_vl, 0, "BANDEC_00459: VHTTPS_post() failed.");
		return (1);
	}
	if (req.resp_status != 200) {
		vtc_log(mwc_vl, 0,
		    "BANDEC_00460: Unexpected response status: %d",
		    req.resp_status);
		return (1);
	}
	resp_body[resp_bodylen] = '\0';
	jroot = json_loads(resp_body, 0, &jerror);
	if (jroot == NULL) {
		vtc_log(mwc_vl, 1,
		    "BANDEC_00461: error while parsing JSON format:"
		    " on line %d: %s", jerror.line, jerror.text);
		vtc_log(mwc_vl, 1,
		    "BANDEC_00462: response body: %s", resp_body);
		return (1);
	}
	jstatus = json_object_get(jroot, "status");
	AN(jstatus);
	assert(json_is_integer(jstatus));
	if (json_integer_value(jstatus) != 200) {
		jmsg = json_object_get(jroot, "msg");
		vtc_log(mwc_vl, 0,
		    "BANDEC_00463: Failed with error: %s",
		    json_string_value(jmsg));
		json_decref(jroot);
		return (1);
	}
	jurl = json_object_get(jroot, "url");
	AN(jurl);
	assert(json_is_string(jurl));
	assert(json_string_length(jurl) > 0);
	vtc_log(mwc_vl, 2,
	    "Please visit the following URL to access Web CLI: %s",
	    json_string_value(jurl));
	return (0);
}

int
MWC_init(void)
{

	mwc_vl = vtc_logopen("webcli", NULL);
	AN(mwc_vl);
	return (0);
}
