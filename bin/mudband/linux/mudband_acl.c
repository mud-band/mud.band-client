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

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
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

static struct vtclog *acl_vl;

static json_t *
acl_request(json_t *jreq_json)
{
	struct vhttps_req req;
	json_error_t jerror;
	json_t *jroot, *jband_jwt, *jstatus, *jmsg;
	size_t resp_bodylen;
	int hdrslen, req_bodylen, r;
	char hdrs[ODR_BUFSIZ];
	char req_body[ODR_BUFSIZ], resp_body[4096];

	AN(mbe_jroot);
	jband_jwt = json_object_get(mbe_jroot, "jwt");
	AN(jband_jwt);

	ODR_bzero(&req, sizeof(req));

	req.f_need_resp_status = 1;
	req.vl = acl_vl;
	req.server = "www.mud.band:443";
	req.domain = "www.mud.band";
	req.url = "/api/band/device/conf";
	hdrslen = ODR_snprintf(hdrs, sizeof(hdrs),
	    "Authorization: %s\r\n"
	    "Content-Type: application/json\r\n"
	    "Host: www.mud.band\r\n", json_string_value(jband_jwt));
	assert(hdrslen < sizeof(hdrs));
	req.hdrs = hdrs;
	{
		char *rb;

		rb = json_dumps(jreq_json, 0);
		AN(rb);
		req_bodylen = ODR_snprintf(req_body, sizeof(req_body), "%s",
		    rb);
		free(rb);
	}
	req.body = req_body;
	req.bodylen = req_bodylen;
	resp_bodylen = sizeof(resp_body);
	r = VHTTPS_post(&req, resp_body, &resp_bodylen);
	if (r == -1) {
		vtc_log(acl_vl, 0, "BANDEC_XXXXX: VHTTPS_post() failed.");
		return (NULL);
	}
	if (req.resp_status != 200) {
		vtc_log(acl_vl, 0,
		    "BANDEC_XXXXX: Unexpected response status: %d",
		    req.resp_status);
		return (NULL);
	}
	resp_body[resp_bodylen] = '\0';
	jroot = json_loads(resp_body, 0, &jerror);
	if (jroot == NULL) {
		vtc_log(acl_vl, 1,
		    "BANDEC_XXXXX: error while parsing JSON format:"
		    " on line %d: %s", jerror.line, jerror.text);
		vtc_log(acl_vl, 1,
		    "BANDEC_XXXXX: response body: %s", resp_body);
		return (NULL);
	}
	jstatus = json_object_get(jroot, "status");
	AN(jstatus);
	assert(json_is_integer(jstatus));
	if (json_integer_value(jstatus) != 200) {
		jmsg = json_object_get(jroot, "msg");
		vtc_log(acl_vl, 0,
		    "BANDEC_XXXXX: Failed with error: %s",
		    json_string_value(jmsg));
		json_decref(jroot);
		return (NULL);
	}
	return (jroot);
}

static int
acl_add(const char *syntax, const char *prioritystr)
{
	json_t *jroot, *jresp;
	int priority = 0;

	if (prioritystr == NULL) {
		vtc_log(acl_vl, 0,
		    "BANDEC_XXXXX: --acl-priority option is required.");
		return (1);
	}
	priority = atoi(prioritystr);
	assert(priority >= 0);
	jroot = json_object();
	AN(jroot);
	json_object_set_new(jroot, "action", json_string("acl_add"));
	json_object_set_new(jroot, "syntax", json_string(syntax));
	json_object_set_new(jroot, "priority", json_integer(priority));
	jresp = acl_request(jroot);
	if (jresp == NULL) {
		vtc_log(acl_vl, 0, "BANDEC_XXXXX: acl_request() failed.");
		json_decref(jroot);
		return (1);
	}
	vtc_log(acl_vl, 2, "Added.");
	json_decref(jroot);
	json_decref(jresp);
	return (0);
}

static int
acl_del(const char *acl_idstr)
{
	json_t *jroot, *jresp;
	int acl_id;

	acl_id = atoi(acl_idstr);
	assert(acl_id >= 0);
	jroot = json_object();
	AN(jroot);
	json_object_set_new(jroot, "action", json_string("acl_del"));
	json_object_set_new(jroot, "acl_id", json_integer(acl_id));
	jresp = acl_request(jroot);
	if (jresp == NULL) {
		vtc_log(acl_vl, 0, "BANDEC_XXXXX: acl_request() failed.");
		json_decref(jroot);
		return (1);
	}
	vtc_log(acl_vl, 2, "Deleted.");
	json_decref(jroot);
	json_decref(jresp);
	return (0);
}

static int
acl_default_policy(const char *arg)
{
	json_t *jroot, *jresp;

	if (!strcasecmp(arg, "allow"))
		arg = "allow";
	else if (!strcasecmp(arg, "block"))
		arg = "block";
	else {
		vtc_log(acl_vl, 0,
		    "BANDEC_XXXXX: Invalid default policy: %s", arg);
		return (1);
	}

	jroot = json_object();
	AN(jroot);
	json_object_set_new(jroot, "action", json_string("acl_default_policy"));
	json_object_set_new(jroot, "default_policy", json_string(arg));
	jresp = acl_request(jroot);
	if (jresp == NULL) {
		vtc_log(acl_vl, 0, "BANDEC_XXXXX: acl_request() failed.");
		json_decref(jroot);
		return (1);
	}
	vtc_log(acl_vl, 2, "Updated the default policy.");
	json_decref(jroot);
	json_decref(jresp);
	return (0);
}

static int
acl_list(void)
{
	json_t *jroot, *jresp, *jacls, *jacls_human_readable;
	size_t i;

	jroot = json_object();
	AN(jroot);
	json_object_set_new(jroot, "action", json_string("acl_list"));
	jresp = acl_request(jroot);
	if (jresp == NULL) {
		vtc_log(acl_vl, 0, "BANDEC_XXXXX: acl_request() failed.");
		json_decref(jroot);
		return (1);
	}
	json_decref(jroot);

	vtc_log(acl_vl, 2, "ACL List:");
	jacls = json_object_get(jresp, "acls");
	AN(jacls);
	assert(json_is_array(jacls));
	for (i = 0; i < json_array_size(jacls); i++) {
		json_t *jacl, *jacl_id, *jcreated, *jsyntax, *jpriority;

		if (i == 0)
			vtc_log(acl_vl, 2, "%08s\t%08s\t%40s\t%s",
			    "ACL ID", "Priority", "Syntax", "Created");

		jacl = json_array_get(jacls, i);
		AN(jacl);
		assert(json_is_object(jacl));

		jacl_id = json_object_get(jacl, "acl_id");
		AN(jacl_id);
		assert(json_is_integer(jacl_id));
		jcreated = json_object_get(jacl, "created");
		AN(jcreated);
		assert(json_is_string(jcreated));
		jpriority = json_object_get(jacl, "priority");
		AN(jpriority);
		assert(json_is_integer(jpriority));
		jsyntax = json_object_get(jacl, "syntax");
		AN(jsyntax);
		assert(json_is_string(jsyntax));

		vtc_log(acl_vl, 2, "%8d\t%8d\t%40s\t%s",
		    (int)json_integer_value(jacl_id),
		    (int)json_integer_value(jpriority),
		    json_string_value(jsyntax),
		    json_string_value(jcreated));
	}
	jacls_human_readable = json_object_get(jresp, "acls_human_readable");
	AN(jacls_human_readable);
	assert(json_is_string(jacls_human_readable));
	vtc_log(acl_vl, 2, "ACL syntax (human readable):");
	vtc_dumpln(acl_vl, 2, json_string_value(jacls_human_readable),
	    json_string_length(jacls_human_readable));

	json_decref(jresp);
	return (0);
}

int
ACL_cmd(const char *acl_add_arg, const char *acl_priority_arg,
    unsigned acl_list_flag, const char *acl_del_arg,
    const char *acl_default_policy_arg)
{
	int r;

	r = MBE_check_and_read();
	if (r == -1) {
		vtc_log(acl_vl, 0, "BANDEC_XXXXX: Enrollment check failed.");
		return (1);
	}
	if (acl_add_arg != NULL)
		return (acl_add(acl_add_arg, acl_priority_arg));
	if (acl_del_arg != NULL)
		return (acl_del(acl_del_arg));
	if (acl_default_policy_arg != NULL)
		return (acl_default_policy(acl_default_policy_arg));
	if (acl_list_flag)
		return (acl_list());
	vtc_log(acl_vl, 0, "BANDEC_XXXXX: Unexpected ACL command.");
	return (1);
}

int
ACL_init(void)
{

	acl_vl = vtc_logopen("acl", mudband_log_printf);
	AN(acl_vl);
	return (0);
}
