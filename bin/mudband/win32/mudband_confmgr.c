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
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "jansson.h"
#include "odr.h"
#include "odr_pthread.h"
#include "vassert.h"
#include "vhttps.h"
#include "vopt.h"
#include "vqueue.h"
#include "vsock.h"
#include "vtc_log.h"

#include "callout.h"

#include "mudband.h"
#include "mudband_stun_client.h"

static odr_pthread_mutex_t cnf_mtx;
static struct cnf *cnf_active; /* protected by cnf_mtx */
static VTAILQ_HEAD(, cnf) cnf_head = VTAILQ_HEAD_INITIALIZER(cnf_head);
static struct vtclog *cnf_vl;

#include <iphlpapi.h>

static json_t *
cnf_getifaddrs(void)
{
	PIP_ADAPTER_ADDRESSES pa = NULL, pc = NULL;
	PIP_ADAPTER_UNICAST_ADDRESS pu = NULL;
	DWORD l = 0;
	DWORD r = 0;
	json_t *jroot;
	uint32_t addr_198_18, mask_198_18;
	uint32_t addr_169_254, mask_169_254;
	char addrstr[INET_ADDRSTRLEN];

	addr_198_18 = inet_addr("198.18.0.0");
	mask_198_18 = inet_addr("255.254.0.0");
	addr_169_254 = inet_addr("169.254.0.0");
	mask_169_254 = inet_addr("255.255.0.0");

	r = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX,
	    NULL, pa, &l);
	if (r == ERROR_BUFFER_OVERFLOW) {
		pa = (PIP_ADAPTER_ADDRESSES)malloc(l);
		if (pa == NULL)
			return (NULL);
		r = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX,
		    NULL, pa, &l);
	}
	if (r != NO_ERROR) {
		if (pa)
			free(pa);
		return (NULL);
	}
	jroot = json_array();
	AN(jroot);
	for (pc = pa; pc != NULL; pc = pc->Next) {
		for (pu = pc->FirstUnicastAddress; pu != NULL; pu = pu->Next) {
			struct sockaddr_in *sin;
			uint32_t addr;
			const char *p;
			
			if (pu->Address.lpSockaddr->sa_family != AF_INET)
				continue;
			sin = (struct sockaddr_in *)pu->Address.lpSockaddr;
			addr = sin->sin_addr.s_addr;
			if (addr == htonl(INADDR_LOOPBACK))
				continue;
			if (addr == htonl(INADDR_ANY))
				continue;
			if (addr == htonl(INADDR_BROADCAST))
				continue;
			if ((addr & mask_198_18) == addr_198_18)
				continue;
			if ((addr & mask_169_254) == addr_169_254)
				continue;
			p = InetNtopA(AF_INET, &sin->sin_addr, addrstr,
			    INET_ADDRSTRLEN);
			if (p == NULL)
				continue;
			json_array_append_new(jroot, json_string(addrstr));
		}
	}
	json_array_append_new(jroot, json_string(STUNC_get_mappped_addr()));
	if (pa)
		free(pa);
	return (jroot);
}

int
CNF_get(struct cnf **cfp)
{

	AZ(ODR_pthread_mutex_lock(&cnf_mtx));
	if (cnf_active == NULL) {
		AZ(ODR_pthread_mutex_unlock(&cnf_mtx));
		return (-1);
	}
	*cfp = cnf_active;
	assert(*cfp != NULL);
	(*cfp)->busy++;
	assert((*cfp)->busy > 0);
	(*cfp)->t_last = time(NULL);
	AZ(ODR_pthread_mutex_unlock(&cnf_mtx));
	return (0);
}

void
CNF_rel(struct cnf **cfp)
{
	struct cnf *cf;

	assert(*cfp != NULL);
	cf = *cfp;
	*cfp = NULL;

	AZ(ODR_pthread_mutex_lock(&cnf_mtx));
	assert(cf->busy > 0);
	cf->busy--;
	AZ(ODR_pthread_mutex_unlock(&cnf_mtx));
}

static int
cnf_file_write(const char *filepath, json_t *obj)
{
	FILE *fp;
	int r;

	fp = fopen(filepath, "w+");
	if (fp == NULL) {
		vtc_log(cnf_vl, 0, "BANDEC_00088: Failed to open file %s: %s",
		    filepath, strerror(errno));
		return (-1);
	}
	r = json_dumpf(obj, fp, 0);
	if (r == -1) {
		vtc_log(cnf_vl, 0,
		    "BANDEC_00089: Failed to write JSON to file %s: %s",
		    filepath, strerror(errno));
		fclose(fp);
		return (-1);
	}
	fclose(fp);
	return (0);
}

const char *
CNF_get_interface_device_uuid(json_t *jroot)
{
	json_t *interface, *device_uuid;

	AN(jroot);
	interface = json_object_get(jroot, "interface");
	AN(interface);
	assert(json_is_object(interface));
	device_uuid = json_object_get(interface, "device_uuid");
	AN(device_uuid);
	assert(json_is_string(device_uuid));
	assert(json_string_length(device_uuid) > 0);
	return (json_string_value(device_uuid));
}

static int
cnf_get_interface_nat_type_by_obj(json_t *jroot)
{
	json_t *interface, *nat_type;

	AN(jroot);
	interface = json_object_get(jroot, "interface");
	AN(interface);
	assert(json_is_object(interface));
	nat_type = json_object_get(interface, "nat_type");
	AN(nat_type);
	assert(json_is_integer(nat_type));
	return ((int)(json_integer_value(nat_type)));
}

static void
cnf_ipv4_verify(const char *addrstr)
{
	struct in_addr in;
	int r;

	r = inet_pton(AF_INET, addrstr, &in);
	assert(r == 1);
}

static const char *
cnf_get_interface_remote_addr_by_obj(json_t *jroot)
{
	json_t *interface, *remote_addr;

	AN(jroot);
	interface = json_object_get(jroot, "interface");
	AN(interface);
	assert(json_is_object(interface));
	remote_addr = json_object_get(interface, "remote_addr");
	AN(remote_addr);
	assert(json_is_string(remote_addr));
	assert(json_string_length(remote_addr) > 0);
	cnf_ipv4_verify(json_string_value(remote_addr));
	return (json_string_value(remote_addr));
}

int
CNF_check_and_read(void)
{
	json_t *jroot;
	json_error_t jerror;
	int nt1, nt2;
	const char *ma1, *ma2;
	char filepath[ODR_BUFSIZ];

	vtc_log(cnf_vl, 2, "Checking the config.");

	ODR_snprintf(filepath, sizeof(filepath), "%s/conf_%s.json",
	    band_confdir_enroll, MBE_get_uuidstr());
	if (ODR_access(filepath, ODR_ACCESS_F_OK) != 0) {
		vtc_log(cnf_vl, 2, "Accesing to %s file failed: %s", filepath,
		    strerror(errno));
		return (-1);
	}
	jroot = json_load_file(filepath, 0, &jerror);
	if (jroot == NULL) {
		vtc_log(cnf_vl, 0, "json_load_file(%s) failed: %d %s",
		    filepath, jerror.line, jerror.text);
		return (-2);
	}
	assert(json_is_object(jroot));
	nt1 = cnf_get_interface_nat_type_by_obj(jroot);
	nt2 = (int)STUNC_get_nattype();
	if (nt1 != nt2) {
		vtc_log(cnf_vl, 2,
		    "NAT type changed. Need to refresh the config.");
		json_decref(jroot);
		return (-3);
	}
	ma1 = STUNC_get_mappped_addr();
	ma2 = cnf_get_interface_remote_addr_by_obj(jroot);
	if (strcmp(ma1, ma2) != 0) {
		vtc_log(cnf_vl, 2,
		    "Mapped address changed (%s -> %s)."
			" Need to refresh the config.",
		    ma1, ma2);
		json_decref(jroot);
		return (-4);
	}
	{
		struct cnf *cnf;

		AZ(ODR_pthread_mutex_lock(&cnf_mtx));
		cnf = calloc(1, sizeof(*cnf));
		AN(cnf);
		cnf->jroot = jroot;
		cnf->t_last = time(NULL);
		VTAILQ_INSERT_TAIL(&cnf_head, cnf, list);
		cnf_active = cnf;
		AZ(ODR_pthread_mutex_unlock(&cnf_mtx));
	}
	if (CNF_get_peer_size(jroot) == 0) {
		vtc_log(cnf_vl, 2,
		    "No peer found. Let's try refresh the config.");
		/* no json_decref here. */
		return (-5);
	}
	vtc_log(cnf_vl, 2, "Completed to read the config.");
	return (0);
}

int
CNF_get_interface_listen_port(json_t *jroot)
{
	json_t *interface, *listen_port;
	int v;

	AN(jroot);
	interface = json_object_get(jroot, "interface");
	AN(interface);
	assert(json_is_object(interface));
	listen_port = json_object_get(interface, "listen_port");
	AN(listen_port);
	assert(json_is_integer(listen_port));
	v = (int)json_integer_value(listen_port);
	assert(v > 0);
	assert(v < 65536);
	return (v);
}

int
CNF_get_peer_size(json_t *jroot)
{
	json_t *jpeers;

	AN(jroot);
	jpeers = json_object_get(jroot, "peers");
	AN(jpeers);
	assert(json_is_array(jpeers));
	return ((int)json_array_size(jpeers));
}

int
CNF_fill_iface_peer(json_t *jroot, struct wireguard_iface_peer *peer,
    int idx)
{
	json_t *jpeers;
	int i, interface_nat_type;

	interface_nat_type = cnf_get_interface_nat_type_by_obj(jroot);
	AN(jroot);
	jpeers = json_object_get(jroot, "peers");
	AN(jpeers);
	assert(json_is_array(jpeers));
	for (i = 0; i < (int)json_array_size(jpeers); i++) {
		json_t *jpeer, *jprivate_ip;
		json_t *jwireguard_pubkey, *jprivate_mask;
		json_t *jdevice_addresses, *jdevice_address;
		json_t *jnat_type;
		int peer_nat_type;
		size_t x;

		if (i != idx)
			continue;
		jpeer = json_array_get(jpeers, i);
		AN(jpeer);
		assert(json_is_object(jpeer));
		/* wireguard_pubkey */
		jwireguard_pubkey = json_object_get(jpeer, "wireguard_pubkey");
		AN(jwireguard_pubkey);
		assert(json_is_string(jwireguard_pubkey));
		assert(json_string_length(jwireguard_pubkey) > 0);
		/* private_ip */
		jprivate_ip = json_object_get(jpeer, "private_ip");
		AN(jprivate_ip);
		assert(json_is_string(jprivate_ip));
		assert(json_string_length(jprivate_ip) > 0);
		cnf_ipv4_verify(json_string_value(jprivate_ip));
		/* private_mask */
		jprivate_mask = json_object_get(jpeer, "private_mask");
		AN(jprivate_mask);
		assert(json_is_string(jprivate_mask));
		assert(json_string_length(jprivate_mask) > 0);
		cnf_ipv4_verify(json_string_value(jprivate_mask));
		/* nat_type */
		jnat_type = json_object_get(jpeer, "nat_type");
		AN(jnat_type);
		assert(json_is_integer(jnat_type));
		peer_nat_type = (int)json_integer_value(jnat_type);
		if (interface_nat_type == 2 /* Open */ &&
		    peer_nat_type == 2 /* Open */) {
			/*
			 * If the interface NAT type is Open and the peer NAT
			 * type is also Open, we don't need to send a keepalive
			 * packet.
			 */
			peer->keep_alive = 0;
		}
		/* device_addresses */
		jdevice_addresses = json_object_get(jpeer, "device_addresses");
		AN(jdevice_addresses);
		assert(json_is_array(jdevice_addresses));
		assert(json_array_size(jdevice_addresses) > 0);
		for (x = 0; x < json_array_size(jdevice_addresses); x++) {
			json_t *jport, *jaddress, *jtype;
			const char *device_address;

			jdevice_address = json_array_get(jdevice_addresses, x);
			AN(jdevice_address);
			assert(json_is_object(jdevice_address));
			/* address */
			jaddress = json_object_get(jdevice_address, "address");
			AN(jaddress);
			assert(json_is_string(jaddress));
			assert(json_string_length(jaddress) > 0);
			cnf_ipv4_verify(json_string_value(jaddress));
			/* port */
			jport = json_object_get(jdevice_address, "port");
			AN(jport);
			assert(json_is_integer(jport));
			/* type */
			jtype = json_object_get(jdevice_address, "type");
			AN(jtype);
			assert(json_is_string(jtype));
			assert(json_string_length(jtype) > 0);
			if (interface_nat_type == 2 /* Open */ &&
			    peer_nat_type == 2 /* Open */ &&
			    !strcmp(json_string_value(jtype), "proxy")) {
				/*
				 * If the interface NAT type is Open and
				 * the peer NAT type is also Open, we don't
				 * need to use proxy.
				 */
				continue;
			}

			device_address = json_string_value(jaddress);
			peer->endpoints[peer->n_endpoints].ip =
			    (uint32_t)inet_addr(device_address);
			peer->endpoints[peer->n_endpoints].port =
			    (uint16_t)json_integer_value(jport);
			peer->endpoints[peer->n_endpoints].is_proxy = false;
			if (!strcmp(json_string_value(jtype), "proxy"))
				peer->endpoints[peer->n_endpoints].is_proxy =
				    true;
			peer->n_endpoints++;
		}
		peer->public_key = json_string_value(jwireguard_pubkey);
		peer->allowed_ip =
		    (uint32_t)inet_addr(json_string_value(jprivate_ip));
		peer->allowed_mask =
		    (uint32_t)inet_addr(json_string_value(jprivate_mask));
		/* XXX */
		peer->iface_addr = peer->allowed_ip;
		return (0);
	}
	return (-1);
}

int
CNF_get_interface_listen_fd(void)
{

	return (MCM_listen_fd());
}

const char *
CNF_get_interface_private_ip(json_t *jroot)
{
	json_t *interface, *private_ip;

	AN(jroot);
	interface = json_object_get(jroot, "interface");
	AN(interface);
	assert(json_is_object(interface));
	private_ip = json_object_get(interface, "private_ip");
	AN(private_ip);
	assert(json_is_string(private_ip));
	assert(json_string_length(private_ip) > 0);
	cnf_ipv4_verify(json_string_value(private_ip));
	return (json_string_value(private_ip));
}

const char *
CNF_get_interface_private_mask(json_t *jroot)
{
	json_t *interface, *private_mask;

	AN(jroot);
	interface = json_object_get(jroot, "interface");
	AN(interface);
	assert(json_is_object(interface));
	private_mask = json_object_get(interface, "private_mask");
	AN(private_mask);
	assert(json_is_string(private_mask));
	assert(json_string_length(private_mask) > 0);
	cnf_ipv4_verify(json_string_value(private_mask));
	return (json_string_value(private_mask));
}

int
CNF_get_interface_mtu(json_t *jroot)
{
	json_t *interface, *mtu;

	AN(jroot);
	interface = json_object_get(jroot, "interface");
	AN(interface);
	assert(json_is_object(interface));
	mtu = json_object_get(interface, "mtu");
	AN(mtu);
	assert(json_is_integer(mtu));
	return ((int)json_integer_value(mtu));
}

static const char *
cnf_get_etag(json_t *jroot)
{
	json_t *jetag;

	AN(jroot);
	jetag = json_object_get(jroot, "etag");
	if (jetag == NULL)
		return (NULL);
	assert(json_is_string(jetag));
	if (json_string_length(jetag) == 0)
		return (NULL);
	return (json_string_value(jetag));
}

struct wireguard_acl *
CNF_acl_build(json_t *jroot)
{
	struct wireguard_acl *acl;
	json_t *jacl, *jprograms, *jdefault_policy;
	size_t i, x;
	int r;

	AN(jroot);

	jacl = json_object_get(jroot, "acl");
	AN(jacl);
	assert(json_is_object(jacl));
	jprograms = json_object_get(jacl, "programs");
	AN(jprograms);
	assert(json_is_array(jprograms));

	acl = calloc(1, sizeof(*acl));
	AN(acl);
	acl->n_programs = json_array_size(jprograms);
	if (acl->n_programs >= WIREGUARD_ACL_PROGRAM_MAX) {
		vtc_log(cnf_vl, 0, "BANDEC_00454: Too many BPF programs: %d",
			acl->n_programs);
		free(acl);
		return (NULL);
	}
	jdefault_policy = json_object_get(jacl, "default_policy");
	AN(jdefault_policy);
	assert(json_is_string(jdefault_policy));
	assert(json_string_length(jdefault_policy) > 0);
	if (!strcmp(json_string_value(jdefault_policy), "allow"))
		acl->default_policy = WIREGUARD_ACL_POLICY_ALLOW;
	else if (!strcmp(json_string_value(jdefault_policy), "block"))
		acl->default_policy = WIREGUARD_ACL_POLICY_BLOCK;
	else {
		vtc_log(cnf_vl, 0, "BANDEC_00455: Invalid default_policy: %s",
		    json_string_value(jdefault_policy));
		free(acl);
		return (NULL);
	}
	for (i = 0; i < json_array_size(jprograms); i++) {
		struct wireguard_acl_program *acl_program;
		json_t *jinsns;

		jinsns = json_array_get(jprograms, i);
		AN(jinsns);
		assert(json_is_array(jinsns));

		acl_program = &acl->programs[i];
		acl_program->n_insns = json_array_size(jinsns);
		if (acl_program->n_insns >= WIREGUARD_ACL_PROGRAM_INSNS_MAX) {
			vtc_log(cnf_vl, 0,
			    "BANDEC_00456: Too many BPF instructions: %d",
			    acl_program->n_insns);
			free(acl);
			return (NULL);
		}
		for (x = 0; x < json_array_size(jinsns); x++) {
			struct mudband_bpf_insn *insn;
			json_t *jinsn;

			jinsn = json_array_get(jinsns, x);
			AN(jinsn);
			assert(json_array_size(jinsn) == 4);
			insn = &acl_program->insns[x];
			insn->code =
			    (uint16_t)json_integer_value(json_array_get(jinsn, 0));
			insn->jt =
			    (uint8_t)json_integer_value(json_array_get(jinsn, 1));
			insn->jf =
			    (uint8_t)json_integer_value(json_array_get(jinsn, 2));
			insn->k =
			    (mudband_bpf_u_int32)json_integer_value(json_array_get(jinsn, 3));
		}
		r = mudband_bpf_validate(acl_program->insns, acl_program->n_insns);
		if (r != 1) {
			vtc_log(cnf_vl, 0,
			    "BANDEC_00457: BPF program validation failed:"
			    " r %d n_insns %d", r, acl_program->n_insns);
			free(acl);
			return (NULL);
		}
	}
	return (acl);
}

int
CNF_fetch(const char *fetch_type)
{
	struct cnf *cnf;
	struct vhttps_req req;
	json_t *jroot, *jband_jwt, *jstatus, *jconf;
	json_error_t jerror;
	size_t resp_bodylen = 1024 * 1024;
	int hdrslen, req_bodylen, r;
	char hdrs[ODR_BUFSIZ], *resp_body;
	char req_body[ODR_BUFSIZ], filepath[ODR_BUFSIZ];
	const char *etag;

	vtc_log(cnf_vl, 2, "Fetching the config for the band ID %s",
	    MBE_get_uuidstr());
	AN(mbe_jroot);
	jband_jwt = json_object_get(mbe_jroot, "jwt");
	AN(jband_jwt);
	assert(json_is_string(jband_jwt));
	ODR_bzero(&req, sizeof(req));
	req.f_need_resp_status = 1;
	req.f_need_resp_mudband_etag = 1;
	req.vl = cnf_vl;
	req.server = "www.mud.band:443";
	req.domain = "www.mud.band";
	req.url = "/api/band/conf";
	hdrslen = ODR_snprintf(hdrs, sizeof(hdrs),
	    "Accept-Encoding: gzip\r\n"
	    "Authorization: %s\r\n"
	    "Content-Type: application/json\r\n"
	    "Host: www.mud.band\r\n", json_string_value(jband_jwt));
	r = CNF_get(&cnf);
	if (r == 0) {
		etag = cnf_get_etag(cnf->jroot);
		if (etag != NULL) {
			ODR_snprintf(hdrs + hdrslen, sizeof(hdrs) - hdrslen,
			    "If-None-Match: %s\r\n", etag);
		}
		CNF_rel(&cnf);
	}
	req.hdrs = hdrs;
	{
		json_t *jreq_body, *jiface, *jifaddrs;
		char *rb;

		jiface = json_object();
		AN(jiface);
		jifaddrs = cnf_getifaddrs();
		AN(jifaddrs);
		jreq_body = json_object();
		AN(jreq_body);
		json_object_set_new(jiface, "listen_port",
		    json_integer(MCM_listen_port()));
		json_object_set_new(jiface, "addresses", jifaddrs);
		json_object_set_new(jreq_body, "interface", jiface);
		json_object_set_new(jreq_body, "stun_nattype",
		    json_integer(STUNC_get_nattype()));
		json_object_set_new(jreq_body, "stun_mapped_addr",
		    json_string(STUNC_get_mappped_addr()));
		json_object_set_new(jreq_body, "fetch_type",
		    json_string(fetch_type));
		rb = json_dumps(jreq_body, 0);
		AN(rb);
		req_bodylen = ODR_snprintf(req_body, sizeof(req_body), "%s",
		    rb);
		free(rb);
		json_decref(jreq_body);
	}
	req.body = req_body;
	req.bodylen = req_bodylen;
	resp_body = malloc(resp_bodylen);
	AN(resp_body);
	r = VHTTPS_post(&req, resp_body, &resp_bodylen);
	if (r == -1) {
		vtc_log(cnf_vl, 0, "BANDEC_00090: VHTTPS_post() failed.");
		free(resp_body);
		return (-1);
	}
	if (req.resp_status == 304) {
		vtc_log(cnf_vl, 2,
		    "No config changed for the band ID %s",
		    MBE_get_uuidstr());
		free(resp_body);
		return (1);
	}
	if (req.resp_status == 503) {
		vtc_log(cnf_vl, 2,
		    "The servser is busy to response for %s."
		    " Try to fetch later %s", MBE_get_uuidstr());
		free(resp_body);
		return (-4);
	}
	resp_body[resp_bodylen] = '\0';
	jroot = json_loads(resp_body, 0, &jerror);
	if (jroot == NULL) {
		vtc_log(cnf_vl, 1,
		    "BANDEC_00091: error while parsing JSON format:"
		    " on line %d: %s", jerror.line, jerror.text);
		vtc_log(cnf_vl, 1,
		    "BANDEC_00092: response body: %s", resp_body);
		free(resp_body);
		return (-2);
	}
	jstatus = json_object_get(jroot, "status");
	AN(jstatus);
	assert(json_is_integer(jstatus));
	if (json_integer_value(jstatus) != 200) {
		json_t *jmsg;

		if (json_integer_value(jstatus) == 301) {
			json_t *jsso_url;

			jsso_url = json_object_get(jroot, "sso_url");
			AN(jsso_url);
			assert(json_is_string(jsso_url));
			vtc_log(cnf_vl, 1,
			    "BANDEC_00845: MFA authentication expired."
			    " Please visit the SSO URL to re-verify: %s",
			    json_string_value(jsso_url));
			band_mfa_authentication_required = 1;
			ODR_snprintf(band_mfa_authentication_url,
			    sizeof(band_mfa_authentication_url),
			    "%s", json_string_value(jsso_url));
			json_decref(jroot);
			free(resp_body);
			return (-4);
		}

		jmsg = json_object_get(jroot, "msg");
		AN(jmsg);
		assert(json_is_string(jmsg));
		vtc_log(cnf_vl, 1,
		    "BANDEC_00093: Error status: %d %s",
		    (int)json_integer_value(jstatus), json_string_value(jmsg));
		json_decref(jroot);
		free(resp_body);
		return (-3);
	}
	jconf = json_object_get(jroot, "conf");
	AN(jconf);
	assert(json_is_object(jconf));
	if (req.resp_mudband_etag[0] != '\0')
		json_object_set_new(jconf, "etag",
		    json_string(req.resp_mudband_etag));
	ODR_snprintf(filepath, sizeof(filepath), "%s/conf_%s.json",
	    band_confdir_enroll, MBE_get_uuidstr());
	r = cnf_file_write(filepath, jconf);
	assert(r == 0);
	vtc_log(cnf_vl, 2, "Completed to fetch the config for the band ID %s",
	    MBE_get_uuidstr());
	json_decref(jroot);
	free(resp_body);
	band_mfa_authentication_required = 0;
	return (0);
}

void
CNF_nuke(void)
{
	struct cnf *cnf, *cnftmp;

	AZ(ODR_pthread_mutex_lock(&cnf_mtx));
	VTAILQ_FOREACH_SAFE(cnf, &cnf_head, list, cnftmp) {
		if (cnf == cnf_active)
		    continue;
		if (cnf->busy > 0)
		    continue;
		VTAILQ_REMOVE(&cnf_head, cnf, list);
		json_decref(cnf->jroot);
		free(cnf);
	}
	AZ(ODR_pthread_mutex_unlock(&cnf_mtx));
}

void
CNF_fini(void)
{
	struct cnf *cnf, *cnftmp;

	AZ(ODR_pthread_mutex_lock(&cnf_mtx));
	VTAILQ_FOREACH_SAFE(cnf, &cnf_head, list, cnftmp) {
		VTAILQ_REMOVE(&cnf_head, cnf, list);
		json_decref(cnf->jroot);
		free(cnf);
	}
	AZ(ODR_pthread_mutex_unlock(&cnf_mtx));
	AZ(ODR_pthread_mutex_destroy(&cnf_mtx));
	vtc_logclose(cnf_vl);
}

int
CNF_init(void)
{

	cnf_vl = vtc_logopen("conf", NULL);
	AN(cnf_vl);
	AZ(ODR_pthread_mutex_init(&cnf_mtx, NULL));
	return (0);
}
