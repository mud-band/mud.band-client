/*
 * Copyright (c) 2021 Daniel Hope (www.floorsense.nz, daniel.hope@smartalock.com)
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

#include <windows.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "wintun.h"

#include "odr.h"
#include "odr_pthread.h"
#include "vassert.h"
#include "vhttps.h"
#include "vopt.h"
#include "vqueue.h"
#include "vtc_log.h"

#include "callout.h"
#include "wireguard-pbuf.h"

#include "mudband.h"
#include "mudband_mqtt.h"
#include "mudband_stun_client.h"

#define	MUDBAND_VERSION		"v0.0.7"

#define WIREGUARD_IFACE_DEFAULT_PORT		(51820)
#define WIREGUARD_IFACE_KEEPALIVE_DEFAULT	(0xFFFF)
#define WIREGUARD_IFACE_INVALID_INDEX		(-1)

#define TODO()		do { assert(0 == 1); } while (0)

#define WIREGUARD_IPHDR_HI_BYTE(byte)	(((byte) >> 4) & 0x0F)
#define WIREGUARD_IPHDR_LO_BYTE(byte)	((byte) & 0x0F)

#pragma pack(push, 4)
struct wireguard_iphdr {
	uint8_t verlen;
	uint8_t tos;
	uint16_t tot_len;
	uint16_t id;
	uint16_t frag_off;
	uint8_t ttl;
	uint8_t protocol;
	uint16_t check;
	uint32_t saddr;
	uint32_t daddr;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct wireguard_proxy_pkthdr {
	uint8_t		f_version : 4,
			unused0 : 4;
	uint8_t		unused1;
	uint8_t		unused2;
	uint8_t		unused3;
	uint8_t		band_uuid[16];
	uint32_t	src_addr;
	uint32_t	dst_addr;
	uint32_t	unused4;
};
#pragma pack(pop)

struct wireguard_sockaddr {
	uint32_t	addr;
	uint16_t	port;
	struct {
		bool		from_it;
		uint32_t	src_addr;
		uint32_t	dst_addr;
	} proxy;
};

struct wireguard_iface_init_data {
	/* Required: the private key of this WireGuard network interface */
	const char *private_key;
	/* Required: What UDP port to listen on */
	int listen_fd;
	const char *private_ip;
};

struct wireguard_iface_stat {
	uint64_t	n_no_peer_found;
	uint64_t	n_no_ipv4_hdr;
	uint64_t	n_pbuf_cache_count;
	uint64_t	n_tun_rx_pkts;
	uint64_t	n_tun_tx_pkts;
	uint64_t	n_udp_rx_pkts;
	uint64_t	n_udp_tx_pkts;
	uint64_t	n_udp_proxy_rx_pkts;
	uint64_t	n_udp_proxy_tx_pkts;
	uint64_t	n_udp_proxy_rx_errs;
	uint64_t	bytes_tun_rx;
	uint64_t	bytes_tun_tx;
	uint64_t	bytes_udp_rx;
	uint64_t	bytes_udp_tx;
	uint64_t	bytes_udp_proxy_rx;
	uint64_t	bytes_udp_proxy_tx;
};
static struct wireguard_iface_stat wg_stat;
static struct callout wg_stat_co;

static struct vtclog *stats_vl;
struct vtclog *band_vl;
const char *band_b_arg;
char *band_confdir_enroll;
char *band_confdir_root;
int band_need_iface_sync = 1;

static struct callout_block wg_cb;
static int wg_aborted;
static int orig_argc;
static char **orig_argv;

#include <iphlpapi.h>

/* wintun funcs */
static WINTUN_CREATE_ADAPTER_FUNC *WintunCreateAdapter;
static WINTUN_CLOSE_ADAPTER_FUNC *WintunCloseAdapter;
static WINTUN_OPEN_ADAPTER_FUNC *WintunOpenAdapter;
static WINTUN_GET_ADAPTER_LUID_FUNC *WintunGetAdapterLUID;
static WINTUN_GET_RUNNING_DRIVER_VERSION_FUNC *WintunGetRunningDriverVersion;
static WINTUN_DELETE_DRIVER_FUNC *WintunDeleteDriver;
static WINTUN_SET_LOGGER_FUNC *WintunSetLogger;
static WINTUN_START_SESSION_FUNC *WintunStartSession;
static WINTUN_END_SESSION_FUNC *WintunEndSession;
static WINTUN_GET_READ_WAIT_EVENT_FUNC *WintunGetReadWaitEvent;
static WINTUN_RECEIVE_PACKET_FUNC *WintunReceivePacket;
static WINTUN_RELEASE_RECEIVE_PACKET_FUNC *WintunReleaseReceivePacket;
static WINTUN_ALLOCATE_SEND_PACKET_FUNC *WintunAllocateSendPacket;
static WINTUN_SEND_PACKET_FUNC *WintunSendPacket;

static WINTUN_ADAPTER_HANDLE band_wintun_adapter;
static WINTUN_SESSION_HANDLE band_wintun_session;
static odr_pthread_t band_wintun_tp;
static int band_wintun_proxy_fd = -1;

static void
mudband_signal_init(void)
{

	/* XXX */
}

static void CALLBACK
wintun_logger(_In_ WINTUN_LOGGER_LEVEL lvl, _In_ DWORD64 timestamp,
    _In_z_ const WCHAR *line)
{
#define	WINTUN_LOGGER_BUFMAX	1024
	unsigned int new_lvl = 0;
	size_t buflen;
	char buf[WINTUN_LOGGER_BUFMAX];

	(void)timestamp;

	switch (lvl) {
	case WINTUN_LOG_INFO:
		new_lvl = 2;
		break;
	case WINTUN_LOG_WARN:
		new_lvl = 1;
		break;
	case WINTUN_LOG_ERR:
		new_lvl = 0;
		break;
	default:
		break;
	}
	buflen = wcstombs(buf, line, sizeof(buf));
	if (buflen >= sizeof(buf))
		buf[WINTUN_LOGGER_BUFMAX - 1] = '\0';
	else {
		if (buflen == (size_t)-1 || buflen <= 0)
			return;
		assert(buflen < sizeof(buf));
		buf[buflen] = '\0';
	}
	if (new_lvl == 0)
		vtc_log(band_vl, new_lvl, "BANDEC_00108: wintun: %s", buf);
	else if (new_lvl == 1)
		vtc_log(band_vl, new_lvl, "BANDEC_00109: wintun: %s", buf);
	else
		vtc_log(band_vl, new_lvl, "wintun: %s", buf);
}

static int
wintun_init(void)
{
	HMODULE wintun;

	wintun = LoadLibraryExW(L"wintun.dll", NULL,
	    LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
	if (!wintun) {
		vtc_log(band_vl, 0,
		    "BANDEC_00110: LoadLibraryExW() failed for wintun.dll: %d",
		    GetLastError());
		return (-1);
	}
#define X(Name) ((*(FARPROC *)&Name = GetProcAddress(wintun, #Name)) == NULL)
	if (X(WintunCreateAdapter) || X(WintunCloseAdapter) ||
	    X(WintunOpenAdapter) || X(WintunGetAdapterLUID) ||
	    X(WintunGetRunningDriverVersion) || X(WintunDeleteDriver) ||
	    X(WintunSetLogger) || X(WintunStartSession) ||
	    X(WintunEndSession) || X(WintunGetReadWaitEvent) ||
	    X(WintunReceivePacket) || X(WintunReleaseReceivePacket) ||
	    X(WintunAllocateSendPacket) || X(WintunSendPacket))
#undef X
	{
		vtc_log(band_vl, 0,
		    "BANDEC_00111: GetProcAddress() failed: %d",
		    GetLastError());
		FreeLibrary(wintun);
		return (-1);
	}
	WintunSetLogger(wintun_logger);
	return (0);
}

static int
wintun_subnet2cidr(const char *netmaskstr)
{
	uint32_t netmask;
	int cidr = 0;

	netmask = inet_addr(netmaskstr);
	while (netmask) {
		cidr += (netmask & 0x1);
		netmask >>= 1;
	}
	return (cidr);
}

static int
wintun_alloc(void)
{
	GUID mudband_guid = { 0x25e70039, 0x33a3, 0x4d60,
		{ 0xb7, 0x71, 0xfc, 0x13, 0x8d, 0x1c, 0xa0, 0x44 } };
	DWORD version;

	band_wintun_adapter = WintunCreateAdapter(L"Mudband", L"Mudband Wintun",
	    &mudband_guid);
	if (!band_wintun_adapter) {
		vtc_log(band_vl, 0,
		    "BANDEC_00112: Failed to create Mudband Wintun adapter %d",
		    GetLastError());
		return (-1);
	}
	version = WintunGetRunningDriverVersion();
	vtc_log(band_vl, 2, "wintun v%u.%u loaded.",
	    (version >> 16) & 0xff, (version >> 0) & 0xff);
	return (0);
}

static void
wintun_free(void)
{

	WintunEndSession(band_wintun_session);
	WintunCloseAdapter(band_wintun_adapter);
}

static void
wintun_cmd(const char *cmd_orig)
{
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	LPSTR cmd;
	BOOL r;

	cmd = _strdup(cmd_orig);
	AN(cmd);
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	r = CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW,
	    NULL, NULL, &si, &pi);
	if (!r) {
		vtc_log(band_vl, 0,
		    "BANDEC_00113: CreateProcess() failed: %d", GetLastError());
		free(cmd);
		return;
	}
	WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	free(cmd);
}

static int
wintun_set_address(const char *private_ip, const char *private_mask, int mtu)
{
	MIB_UNICASTIPADDRESS_ROW address_row;
	DWORD ifindex = -1, rv;

	AN(band_wintun_adapter);

	InitializeUnicastIpAddressEntry(&address_row);
	WintunGetAdapterLUID(band_wintun_adapter, &address_row.InterfaceLuid);
	rv = ConvertInterfaceLuidToIndex(&address_row.InterfaceLuid,
	    &ifindex);
	assert(rv == NO_ERROR);
	assert(ifindex != -1);
	address_row.Address.Ipv4.sin_family = AF_INET;
	address_row.Address.Ipv4.sin_addr.S_un.S_addr = inet_addr(private_ip);
	address_row.OnLinkPrefixLength = wintun_subnet2cidr(private_mask);
	address_row.DadState = IpDadStatePreferred;
	rv = CreateUnicastIpAddressEntry(&address_row);
	if (rv != ERROR_SUCCESS && rv != ERROR_OBJECT_ALREADY_EXISTS) {
		vtc_log(band_vl, 0,
		    "BANDEC_00114: Failed to set IP address: %d", rv);
		return (-1);
	}
	{
		DWORD status;
		char tmp[1024], sysroot[512];

		status = GetEnvironmentVariableA("SystemRoot", sysroot,
		    sizeof(sysroot));
		assert(status != 0);
		ODR_snprintf(tmp, sizeof(tmp),
		    "%s\\system32\\netsh.exe interface ipv4 set interface"
		    " %d mtu=%d",
		    sysroot, ifindex, mtu);
		wintun_cmd(tmp);
	}
	band_wintun_session = WintunStartSession(band_wintun_adapter, 0x400000);
	if (!band_wintun_session) {
		vtc_log(band_vl, 0,
		    "BANDEC_00115: Failed to create adapter");
		return (-1);
	}
	return (0);
}

static void *
wintun_thread(void *arg)
{
	struct sockaddr_in sin;
	int sinlen;
	HANDLE wait_handle;
	DWORD pktlen;
	int r;
	BYTE *pkt;
	
	(void)arg;
	AN(band_wintun_session);
	assert(band_wintun_proxy_fd > 0);

	sinlen = sizeof(sin);
	r = getsockname(band_wintun_proxy_fd, (struct sockaddr *)&sin, &sinlen);
	assert(r == 0);

	while (1) {
		wait_handle = WintunGetReadWaitEvent(band_wintun_session);
		pktlen = 0;
		pkt = WintunReceivePacket(band_wintun_session, &pktlen);
		if (pkt == NULL) {
			DWORD status, last_error = GetLastError();

			switch (last_error) {
			case ERROR_NO_MORE_ITEMS:
				status = WaitForSingleObject(wait_handle,
				    300);
				if (status == WAIT_TIMEOUT)
					continue;
				if (status == WAIT_OBJECT_0)
					continue;
				vtc_log(band_vl, 0,
				    "BANDEC_00116:"
				    " WaitForSingleObject() failed: %d %s",
				    status, ODR_strerror(status));
				break;
			default:
				vtc_log(band_vl, 0,
				    "BANDEC_00117:"
				    " WintunReceivePacket() failed: %d %s",
				    last_error, ODR_strerror(last_error));
				break;
			}
			continue;
		}
		assert(pktlen > 0);
		r = sendto(band_wintun_proxy_fd, pkt, pktlen, 0,
		    (struct sockaddr *)&sin, sinlen);
		assert(r == pktlen);
		WintunReleaseReceivePacket(band_wintun_session, pkt);
	}

	return (NULL);
}

static int
mudband_tunnel_iface_init(const char *private_ip, const char *private_mask,
    int mtu)
{
	struct sockaddr_in sin;
	int fd, r;

	r = wintun_init();
	if (r != 0) {
		vtc_log(band_vl, 0, "BANDEC_00450: wintun_init() failed.");
		return (-1);
	}
	r = wintun_alloc();
	if (r != 0) {
		vtc_log(band_vl, 0, "BANDEC_00451: wintun_alloc() failed.");
		return (-1);
	}
	r = wintun_set_address(private_ip, private_mask, mtu);
	if (r != 0) {
		vtc_log(band_vl, 0,
		    "BANDEC_00452: wintun_set_address() failed.");
		return (-1);
	}
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	assert(fd != SOCKET_ERROR);
	ODR_bzero(&sin, sizeof(sin));
	sin.sin_addr.s_addr = inet_addr("127.0.0.1");
	sin.sin_port = htons(0); 
	sin.sin_family = AF_INET;
	r = bind(fd, (struct sockaddr *)&sin, sizeof(sin));
	assert(r != SOCKET_ERROR);
	band_wintun_proxy_fd = fd;

	AZ(ODR_pthread_create(&band_wintun_tp, NULL, wintun_thread, NULL));
	AZ(ODR_pthread_detach(band_wintun_tp));
	return (fd);
}

static void
mudband_tunnel_iface_fini(void)
{

	if (band_wintun_proxy_fd >= 0) {
		closesocket(band_wintun_proxy_fd);
		band_wintun_proxy_fd = -1;
	}
	wintun_free();
}

static void
mudband_tunnel_iface_write(uint8_t *buf, size_t buflen)
{
	BYTE *pkt;

	AN(band_wintun_session);
	
	pkt = WintunAllocateSendPacket(band_wintun_session, buflen);
	AN(pkt);
	ODR_bcopy(buf, pkt, buflen);
	WintunSendPacket(band_wintun_session, pkt);
}

uint32_t
wireguard_sys_now(void)
{
	struct odr_timespec ts;

	assert(ODR_clock_gettime(ODR_CLOCK_MONOTONIC, &ts) == 0);
	return (uint32_t)(ts.tv_sec * 1000L + ts.tv_nsec / 1000000L);
}

void
wireguard_random_bytes(void *bytes, size_t size)
{
	int x;
	uint8_t *out = (uint8_t *)bytes;

	for (x = 0; x < (int)size; x++) {
		out[x] = rand() % 0xFF;
	}
}

/*
 * See https://cr.yp.to/libtai/tai64.html
 *
 * 64 bit seconds from 1970 = 8 bytes
 * 32 bit nano seconds from current second.
 */
void
wireguard_tai64n_now(uint8_t *output)
{
	uint64_t millis, seconds;
	uint32_t nanos;

	millis = (uint64_t)wireguard_sys_now();
	/* Split into seconds offset + nanos */
	seconds = 0x400000000000000aULL + (millis / 1000);
	nanos = (millis % 1000) * 1000;
	U64TO8_BIG(output + 0, seconds);
	U32TO8_BIG(output + 8, nanos);
}

bool
wireguard_is_under_load(void)
{

	return (false);
}

static bool
wireguard_iface_can_send_initiation(struct wireguard_peer *peer)
{

	return ((peer->last_initiation_tx == 0) ||
	    (wireguard_expired(peer->last_initiation_tx, WIREGUARD_REKEY_TIMEOUT)));
}

static bool
wireguard_iface_should_send_initiation(struct wireguard_peer *peer)
{
	bool result = false;

	if (wireguard_iface_can_send_initiation(peer)) {
		if (peer->send_handshake) {
			result = true;
		} else if (peer->curr_keypair.valid &&
		    !peer->curr_keypair.initiator &&
		    wireguard_expired(peer->curr_keypair.keypair_millis, WIREGUARD_REJECT_AFTER_TIME - peer->keepalive_interval)) {
			result = true;
		} else if (!peer->curr_keypair.valid && peer->active) {
			result = true;
		}
	}
	return result;
}

static bool
wireguard_iface_should_send_keepalive(struct wireguard_peer *peer)
{
	bool result = false;

	if (peer->keepalive_interval > 0) {
		if ((peer->curr_keypair.valid) || (peer->prev_keypair.valid)) {
			if (wireguard_expired(peer->last_tx, peer->keepalive_interval)) {
				result = true;
			}
		}
	}
	return result;
}

static bool
wireguard_iface_should_destroy_current_keypair(struct wireguard_peer *peer)
{
	bool result = false;

	if (peer->curr_keypair.valid &&
	    (wireguard_expired(peer->curr_keypair.keypair_millis, WIREGUARD_REJECT_AFTER_TIME) ||
		(peer->curr_keypair.sending_counter >= WIREGUARD_REJECT_AFTER_MESSAGES))) {
		result = true;
	}
	return result;
}

static bool
wireguard_iface_should_reset_peer(struct wireguard_peer *peer)
{
	bool result = false;

	if (peer->curr_keypair.valid &&
	    (wireguard_expired(peer->curr_keypair.keypair_millis, WIREGUARD_REJECT_AFTER_TIME * 3))) {
		result = true;
	}
	return result;
}

static uint8_t *
wireguard_iface_prepend_proxy_pkthdr(uint8_t *buf, size_t *buflen,
    uint32_t src_addr, uint32_t dst_addr)
{
	struct wireguard_proxy_pkthdr *hdr;
	const vuuid_t *band_uuid;

	AN(buf);
	AN(buflen);
	hdr = (struct wireguard_proxy_pkthdr *)(buf - sizeof(*hdr));
	memset(hdr, 0, sizeof(*hdr));
	band_uuid = MBE_get_uuid();
	assert(sizeof(vuuid_t) == sizeof(hdr->band_uuid));
	hdr->f_version = 1;
	memcpy(hdr->band_uuid, band_uuid, sizeof(hdr->band_uuid));
	hdr->src_addr = src_addr;
	hdr->dst_addr = dst_addr;
	*buflen += sizeof(*hdr);
	return ((uint8_t *)hdr);
}

static int
wireguard_iface_peer_output_multipath(struct wireguard_device *device,
    struct pbuf *pbuf, struct wireguard_peer *pr)
{
	struct sockaddr_in sin;
	size_t buflen = pbuf->len;
	ssize_t l;
	int same_endpoint = 0, x;
	uint8_t *buf = pbuf->payload;

	for (x = 0 ; x < pr->n_endpoints; x++) {
		if (pr->endpoints[x].ip == pr->endpoint_latest_ip &&
		    pr->endpoints[x].port == pr->endpoint_latest_port &&
		    pr->endpoints[x].is_proxy == pr->endpoint_latest_is_proxy) {
			same_endpoint = 1;
		}
		if (pr->endpoints[x].is_proxy) {
			buf = wireguard_iface_prepend_proxy_pkthdr(buf, &buflen,
			    device->iface_addr, pr->iface_addr);
			wg_stat.n_udp_proxy_tx_pkts++;
			wg_stat.bytes_udp_proxy_tx += buflen;
		}
		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = pr->endpoints[x].ip;
		sin.sin_port = htons(pr->endpoints[x].port);
		l = sendto(device->udp_fd, buf, buflen, 0,
		    (struct sockaddr *)&sin, sizeof(sin));
		if (l == -1) {
			vtc_log(band_vl, 0,
			    "BANDEC_00074: sendto(2) to %s:%d failed: %d %s",
			    inet_ntoa(sin.sin_addr), pr->endpoints[x].port,
			    errno, strerror(errno));
			return (-1);
		}
		assert(l == buflen);
		wg_stat.n_udp_tx_pkts++;
		wg_stat.bytes_udp_tx += buflen;
	}
	if (same_endpoint)
		return (1);
	return (0);
}

static int
wireguard_iface_peer_output(struct wireguard_device *device, struct pbuf *q,
    struct wireguard_peer *peer, bool need_multipath)
{
	struct sockaddr_in sin;
	size_t buflen = q->len;
	ssize_t l;
	int r;
	uint8_t *buf = q->payload;

	if (need_multipath) {
		r = wireguard_iface_peer_output_multipath(device, q, peer);
		if (r == 1)
			return (0);
		/* Send to the default endpoint */
	}
	if (peer->endpoint_latest_is_proxy) {
		buf = wireguard_iface_prepend_proxy_pkthdr(buf, &buflen,
		    device->iface_addr, peer->iface_addr);
		wg_stat.n_udp_proxy_tx_pkts++;
		wg_stat.bytes_udp_proxy_tx += buflen;
	}
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = peer->endpoint_latest_ip;
	sin.sin_port = htons(peer->endpoint_latest_port);
	l = sendto(device->udp_fd, buf, buflen, 0,
	    (struct sockaddr *)&sin, sizeof(sin));
	if (l == -1) {
		TODO();
		return (-1);
	}
	assert(l == buflen);
	wg_stat.n_udp_tx_pkts++;
	wg_stat.bytes_udp_tx += buflen;
	return (0);
}

static int
wireguard_iface_device_output(struct wireguard_device *device, struct pbuf *q,
    struct wireguard_sockaddr *wsin)
{
	struct sockaddr_in sin;
	size_t buflen = q->len;
	ssize_t l;
	uint8_t *buf = q->payload;

	if (wsin->proxy.from_it) {
		buf = wireguard_iface_prepend_proxy_pkthdr(buf, &buflen,
		    device->iface_addr, wsin->proxy.src_addr);		
		wg_stat.n_udp_proxy_tx_pkts++;
		wg_stat.bytes_udp_proxy_tx += buflen;
	}
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = wsin->addr;
	sin.sin_port = htons(wsin->port);
	l = sendto(device->udp_fd, buf, buflen, 0,
	    (struct sockaddr *)&sin, sizeof(sin));
	if (l == -1) {
		TODO();
		return (-1);
	}
	assert(l == buflen);
	wg_stat.n_udp_tx_pkts++;
	wg_stat.bytes_udp_tx += buflen;
	return (0);
}

static int
wireguard_iface_output_to_peer(struct wireguard_device *device, struct pbuf *p,
    struct wireguard_peer *peer)
{
	struct wireguard_msg_transport_data *hdr;
	struct pbuf *pbuf;
	struct wireguard_keypair *keypair = &peer->curr_keypair;
	int result;
	size_t unpadded_len;
	size_t padded_len;
	size_t header_len = 16;
	uint8_t *dst;
	uint32_t now;

	// Note: We may not be able to use the current keypair if
	// we haven't received data, may need to resort to using
	// previous keypair
	if (keypair->valid && (!keypair->initiator) && (keypair->last_rx == 0)) {
		keypair = &peer->prev_keypair;
	}
	if (keypair->valid && (keypair->initiator || keypair->last_rx != 0)) {
		if (!wireguard_expired(keypair->keypair_millis, WIREGUARD_REJECT_AFTER_TIME) &&
		    (keypair->sending_counter < WIREGUARD_REJECT_AFTER_MESSAGES)) {
			// Calculate the outgoing packet size - round up to
			// next 16 bytes, add 16 bytes for header
			if (p) {
				// This is actual transport data
				unpadded_len = p->len;
			} else {
				// This is a keep-alive
				unpadded_len = 0;
			}
			// Round up to next 16 byte boundary
			padded_len = (unpadded_len + 15) & 0xFFFFFFF0;

			// The IP packet consists of 16 byte header
			// (struct wireguard_msg_transport_data), data padded
			// upto 16 byte boundary + encrypted auth tag (16 bytes)
			pbuf = pbuf_alloc(header_len + padded_len +
			    WIREGUARD_AUTHTAG_LEN);
			if (pbuf == NULL) {
				TODO();
				return (-1);
			}
			memset(pbuf->payload, 0, sizeof(*hdr));
			hdr = (struct wireguard_msg_transport_data *)pbuf->payload;
			hdr->type = WIREGUARD_MSG_TRANSPORT_DATA;
			hdr->receiver = keypair->remote_index;
			// Alignment required... pbuf_alloc has probably
			// aligned data, but want to be sure
			U64TO8_LITTLE(hdr->counter, keypair->sending_counter);

			// Copy the encrypted (padded) data to the output
			// packet - chacha20poly1305_encrypt() can encrypt
			// data in-place which avoids call to mem_malloc
			dst = &hdr->enc_packet[0];
			if ((padded_len > 0) && p) {
				// Note: before copying make sure we have
				// inserted the IP header checksum
				// The IP header checksum (and other checksums
				// in the IP packet - e.g. ICMP) need to be
				// calculated by LWIP before calling
				// The Wireguard interface always needs
				// checksums to be generated in software but
				// the base netif may have some checksums
				// generated by hardware

				// Copy pbuf to memory - handles case where
				// pbuf is chained
				pbuf_copy_partial(p, dst, unpadded_len, 0);
			}
			wireguard_encrypt_packet(dst, dst, padded_len, keypair);
			result = wireguard_iface_peer_output(device, pbuf, peer,
			    false);
			if (result == 0) {
				now = wireguard_sys_now();
				peer->last_tx = now;
				keypair->last_tx = now;
			}
			pbuf_free(pbuf);
			// Check to see if we should rekey
			if (keypair->sending_counter >= WIREGUARD_REKEY_AFTER_MESSAGES) {
				peer->send_handshake = true;
			} else if (keypair->initiator &&
			    wireguard_expired(keypair->keypair_millis, WIREGUARD_REKEY_AFTER_TIME)) {
				peer->send_handshake = true;
			}
		} else {
			// key has expired...
			wireguard_keypair_destroy(keypair);
			return (-1);
		}
		return (0);
	}
	// No valid keys!
	return (-1);
}

static void
wireguard_iface_send_keepalive(struct wireguard_device *device,
    struct wireguard_peer *peer)
{

	// Send a NULL packet as a keep-alive
	wireguard_iface_output_to_peer(device, NULL, peer);
}

static struct pbuf *
wireguard_iface_initiate_handshake(struct wireguard_device *device,
    struct wireguard_peer *peer, struct wireguard_msg_handshake_initiation *msg,
    int *error)
{
	struct pbuf *pbuf = NULL;
	int err = 0;

	if (wireguard_create_handshake_initiation(device, peer, msg)) {
		pbuf = pbuf_alloc(sizeof(struct wireguard_msg_handshake_initiation));
		AN(pbuf);
		err = pbuf_take(pbuf, msg,
		    sizeof(struct wireguard_msg_handshake_initiation));
		assert(err == 0);
	}
	if (error) {
		*error = err;
	}
	return (pbuf);
}

static int
wireguard_start_handshake(struct wireguard_device *device,
    struct wireguard_peer *peer)
{
	struct wireguard_msg_handshake_initiation msg;
	struct pbuf *pbuf;
	int result = -1;

	pbuf = wireguard_iface_initiate_handshake(device, peer, &msg, &result);
	if (pbuf) {
		result = wireguard_iface_peer_output(device, pbuf, peer, true);
		pbuf_free(pbuf);
		peer->send_handshake = false;
		peer->last_initiation_tx = wireguard_sys_now();
		memcpy(peer->handshake_mac1, msg.mac1, WIREGUARD_COOKIE_LEN);
		peer->handshake_mac1_valid = true;
	}
	return result;
}

static void
wireguard_iface_timer(void *arg)
{
	struct wireguard_device *device = (struct wireguard_device *)arg;
	struct wireguard_peer *peer;
	int x;
	bool activated = false;

	(void)activated;

	for (x = 0; x < device->peers_count; x++) {
		peer = &device->peers[x];
		if (!peer->valid)
			continue;
		// Do we need to rekey / send a handshake?
		if (wireguard_iface_should_reset_peer(peer)) {
			// Nothing back for too long -
			// we should wipe out all crypto state
			wireguard_keypair_destroy(&peer->next_keypair);
			wireguard_keypair_destroy(&peer->curr_keypair);
			wireguard_keypair_destroy(&peer->prev_keypair);
			// TODO: Also destroy handshake?
			
			// Revert back to default IP/port if these were altered
			peer->endpoint_latest_is_proxy =
			    peer->endpoints[0].is_proxy;
			peer->endpoint_latest_ip = peer->endpoints[0].ip;
			peer->endpoint_latest_port = peer->endpoints[0].port;
		}
		if (wireguard_iface_should_destroy_current_keypair(peer)) {
			// Destroy current keypair
			wireguard_keypair_destroy(&peer->curr_keypair);
		}
		if (wireguard_iface_should_send_keepalive(peer)) {
			wireguard_iface_send_keepalive(device, peer);
		}
		if (wireguard_iface_should_send_initiation(peer)) {
			wireguard_start_handshake(device, peer);
		}
		if (peer->curr_keypair.valid || peer->prev_keypair.valid) {
			activated = true;
		}
	}

	callout_reset(&wg_cb, &device->co, CALLOUT_MSTOTICKS(400),
	    wireguard_iface_timer, device);
}

static struct wireguard_device *
wireguard_iface_init(struct wireguard_iface_init_data *init_data)
{
	struct wireguard_device *device;
	uint8_t private_key[WIREGUARD_PRIVATE_KEY_LEN];
	size_t private_key_len = sizeof(private_key);
	bool r;

	r = wireguard_base64_decode(init_data->private_key, private_key,
	    &private_key_len);
	if (!r) {
		TODO();
		return (NULL);
	}
	if (private_key_len != WIREGUARD_PRIVATE_KEY_LEN) {
		TODO();
		return (NULL);
	}
	device = (struct wireguard_device *)calloc(1, sizeof(*device));
	AN(device);
	AN(init_data->private_ip);
	device->iface_addr = (uint32_t)inet_addr(init_data->private_ip);
	device->udp_fd = init_data->listen_fd;
	callout_init(&device->co, 0);
	device->peers_count = 0;

	// Per-wireguard netif/device setup
	r = wireguard_device_init(device, private_key);
	if (!r) {
		TODO();
		return (NULL);
	}

	vtc_log(band_vl, 2, "Initialized the wireguard device.");

	callout_reset(&wg_cb, &device->co, CALLOUT_MSTOTICKS(400),
	    wireguard_iface_timer, device);
	return (device);
}

static void
wireguard_iface_fini(struct wireguard_device *device)
{

	callout_stop(&wg_cb, &device->co);
	if (device->peers != NULL)
		free(device->peers);
	mudband_tunnel_iface_fini();
	if (device->udp_fd >= 0)
		ODR_close(device->udp_fd);
	free(device);
}

static void
wireguard_iface_peer_init(struct wireguard_iface_peer *peer)
{
	uint8_t i;

	AN(peer);

	memset(peer, 0, sizeof(struct wireguard_iface_peer));
	peer->public_key = NULL;
	for (i = 0; i < WIREGUARD_IFACE_PEER_ENDPOINTS_MAX; i++) {
		peer->endpoints[i].is_proxy = false;
		peer->endpoints[i].ip = INADDR_ANY;
		peer->endpoints[i].port = WIREGUARD_IFACE_DEFAULT_PORT;
	}
	peer->keep_alive = WIREGUARD_IFACE_KEEPALIVE_DEFAULT;
	peer->allowed_ip = INADDR_ANY;
	peer->allowed_mask = INADDR_ANY;
	memset(peer->greatest_timestamp, 0, sizeof(peer->greatest_timestamp));
	peer->preshared_key = NULL;
}

static bool
wireguard_iface_peer_add_ip(struct wireguard_peer *peer, uint32_t ip,
    uint32_t mask)
{
	struct wireguard_allowed_ip *allowed;
	bool result = false;
	int x;

	/* Look for existing match first */
	for (x=0; x < WIREGUARD_MAX_SRC_IPS; x++) {
		allowed = &peer->allowed_source_ips[x];
		if (allowed->valid &&
		    allowed->ip == ip &&
		    allowed->mask == mask) {
			result = true;
			break;
		}
	}
	if (!result) {
		/* Look for a free slot */
		for (x=0; x < WIREGUARD_MAX_SRC_IPS; x++) {
			allowed = &peer->allowed_source_ips[x];
			if (!allowed->valid) {
				allowed->valid = true;
				allowed->ip = ip;
				allowed->mask = mask;
				result = true;
				break;
			}
		}
	}
	return result;
}

static struct wireguard_peer *
wireguard_iface_reusable_old_peer_by_pubkey(struct wireguard_peer *peers,
    int peers_count, uint8_t *public_key)
{
	struct wireguard_peer *result = NULL;
	struct wireguard_peer *tmp;
	int r, x;

	for (x = 0; x < peers_count; x++) {
		tmp = &peers[x];
		if (tmp->valid) {
			r = memcmp(tmp->public_key, public_key,
			    WIREGUARD_PUBLIC_KEY_LEN);
			if (r == 0) {
				result = tmp;
				break;
			}
		}
	}
	return result;
}

static struct wireguard_peer *
wireguard_iface_reusable_old_peer(struct wireguard_peer *peers,
    int peers_count, struct wireguard_iface_peer *p)
{
	struct wireguard_peer *peer = NULL;
	size_t public_key_len;
	uint8_t i, public_key[WIREGUARD_PUBLIC_KEY_LEN];
	bool r;

	public_key_len = sizeof(public_key);
	r = wireguard_base64_decode(p->public_key, public_key, &public_key_len);
	if (!r || public_key_len != WIREGUARD_PUBLIC_KEY_LEN)
		return (NULL);
	peer = wireguard_iface_reusable_old_peer_by_pubkey(peers,
	    peers_count, public_key);
	if (peer == NULL)
		return (NULL);
	if (peer->n_endpoints != p->n_endpoints)
		return (NULL);
	for (i = 0; i < peer->n_endpoints; i++) {
		if (peer->endpoints[i].is_proxy != p->endpoints[i].is_proxy)
			return (NULL);
		if (peer->endpoints[i].ip != p->endpoints[i].ip)
			return (NULL);
		if (peer->endpoints[i].port != p->endpoints[i].port)
			return (NULL);
	}
	return (peer);
}

static int
wireguard_iface_add_peer(struct wireguard_device *device,
    struct wireguard_iface_peer *p, int *peer_index)
{
	struct wireguard_peer *peer = NULL;
	size_t public_key_len;
	uint8_t i, public_key[WIREGUARD_PUBLIC_KEY_LEN];
	bool r;

	AN(peer_index);

	*peer_index = WIREGUARD_IFACE_INVALID_INDEX;
	public_key_len = sizeof(public_key);

	r = wireguard_base64_decode(p->public_key, public_key, &public_key_len);
	if (!r || public_key_len != WIREGUARD_PUBLIC_KEY_LEN) {
		vtc_log(band_vl, 0, "BANDEC_00075: Invalid public key %s",
		    p->public_key);
		return (-1);
	}
	/* See if the peer is already registered */
	peer = wireguard_peer_lookup_by_pubkey(device, public_key);
	if (peer != NULL) {
		*peer_index = wireguard_peer_index(device, peer);
		return (0);
	}
	/* Not active - see if we have room to allocate a new one */
	peer = wireguard_peer_alloc(device);
	if (peer == NULL) {
		vtc_log(band_vl, 0, "BANDEC_00076: No room for new peer");
		return (-1);
	}
	r = wireguard_peer_init(device, peer, public_key, p->preshared_key);
	if (!r) {
		vtc_log(band_vl, 0,
		    "BANDEC_00077: wireguard_peer_init() failed");
		return (-1);
	}
	peer->iface_addr = p->iface_addr;
	for (i = 0; i < p->n_endpoints; i++) {
		peer->endpoints[i].alive = false;
		peer->endpoints[i].is_proxy = p->endpoints[i].is_proxy;
		peer->endpoints[i].ip = p->endpoints[i].ip;
		peer->endpoints[i].port = p->endpoints[i].port;
	}
	peer->n_endpoints = p->n_endpoints;
	peer->endpoint_latest_ip = peer->endpoints[0].ip;
	peer->endpoint_latest_port = peer->endpoints[0].port;
	peer->endpoint_latest_is_proxy = peer->endpoints[0].is_proxy;
	if (p->keep_alive == WIREGUARD_IFACE_KEEPALIVE_DEFAULT) {
		peer->keepalive_interval = WIREGUARD_KEEPALIVE_TIMEOUT;
	} else {
		peer->keepalive_interval = p->keep_alive;
	}
	r = wireguard_iface_peer_add_ip(peer, p->allowed_ip, p->allowed_mask);
	assert(r);
	memcpy(peer->greatest_timestamp, p->greatest_timestamp,
	    sizeof(peer->greatest_timestamp));
	*peer_index = wireguard_peer_index(device, peer);
	{
		struct in_addr in;

		in.s_addr = peer->iface_addr;
		vtc_log(band_vl, 2,
		    "Added a peer (private_ip %s idx %d n_endpoints %d)",
		    inet_ntoa(in), *peer_index, peer->n_endpoints);
	}
	return (0);
}

static int
wireguard_iface_lookup_peer(struct wireguard_device *device, int peer_index,
    struct wireguard_peer **out)
{
	struct wireguard_peer *peer = NULL;

	if (device->valid) {
		peer = wireguard_peer_lookup_by_peer_index(device, peer_index);
		if (peer) {
			*out = peer;
			return (0);
		}
	}
	return (-1);
}

static int
wireguard_iface_connect(struct wireguard_device *device, int peer_index)
{
	struct wireguard_peer *peer;
	int result;

	result = wireguard_iface_lookup_peer(device, peer_index, &peer);
	if (result == -1) {
		vtc_log(band_vl, 0,
		    "BANDEC_00078: wireguard_iface_lookup_peer() failed");
		return (-1);
	}
	/* Check that a valid connect ip and port have been set */
	if (peer->endpoints[0].ip == INADDR_ANY ||
	    peer->endpoints[0].port == 0) {
		vtc_log(band_vl, 0,
		    "BANDEC_00079: Invalid endpoint ip/port for peer");
		return (-1);
	}
	/* Set the flag that we want to try connecting */
	peer->active = true;
	peer->endpoint_latest_ip = peer->endpoints[0].ip;
	peer->endpoint_latest_port = peer->endpoints[0].port;
	peer->endpoint_latest_is_proxy = peer->endpoints[0].is_proxy;
	return (0);
}

static struct wireguard_peer *
wireguard_iface_peer_lookup_by_allowed_ip(struct wireguard_device *device,
    const uint32_t ipaddr)
{
	struct wireguard_peer *result = NULL;
	struct wireguard_peer *tmp;
	int x;
	int y;

	for (x = 0; (!result) && (x < device->peers_count); x++) {
		tmp = &device->peers[x];
		if (!tmp->valid)
			continue;
		for (y = 0; y < WIREGUARD_MAX_SRC_IPS; y++) {
			uint32_t v1, v2;

			if (!(tmp->allowed_source_ips[y].valid))
				continue;
			v1 = ipaddr & tmp->allowed_source_ips[y].mask;
			v2 = tmp->allowed_source_ips[y].ip &
			    tmp->allowed_source_ips[y].mask;
			if (v1 == v2) {
				result = tmp;
				break;
			}
		}
	}
	return result;
}

static int
wireguard_iface_output(struct wireguard_device *device, struct pbuf *p,
    const uint32_t ipaddr)
 {
	struct wireguard_peer *peer;

	peer = wireguard_iface_peer_lookup_by_allowed_ip(device, ipaddr);
	if (peer == NULL) {
		/* No peer found - drop packet */
		wg_stat.n_no_peer_found++;
		return (-1);
	}
	return wireguard_iface_output_to_peer(device, p, peer);
}

static size_t
wireguard_iface_get_source_addr_port(const uint32_t addr, uint16_t port,
    uint8_t *buf, size_t buflen)
{
	size_t result = 0;

	U32TO8_BIG(buf + result, ntohl(addr));
	result += 4;
	if (buflen >= result + 2) {
		U16TO8_BIG(buf + result, port);
		result += 2;
	}
	return result;
}

static void
wireguard_iface_send_handshake_cookie(struct wireguard_device *device,
    const uint8_t *mac1, uint32_t index, struct wireguard_sockaddr *wsin)
{
	struct wireguard_msg_cookie_reply packet;
	struct pbuf *pbuf = NULL;
	int err;
	uint8_t src_buf[18];
	size_t src_len;

	src_len = wireguard_iface_get_source_addr_port(wsin->addr, wsin->port,
	    src_buf, sizeof(src_buf));
	wireguard_create_cookie_reply(device, &packet, mac1, index,
	    src_buf, src_len);

	/* Send this packet out! */
	pbuf = pbuf_alloc(sizeof(struct wireguard_msg_cookie_reply));
	if (pbuf) {
		err = pbuf_take(pbuf, &packet,
		    sizeof(struct wireguard_msg_cookie_reply));
		assert(err == 0);
		wireguard_iface_device_output(device, pbuf, wsin);
		pbuf_free(pbuf);
	}
}

static bool
wireguard_iface_check_initiation_message(struct wireguard_device *device,
    struct wireguard_msg_handshake_initiation *msg,
    struct wireguard_sockaddr *wsin)
{
	bool result = false;
	uint8_t *data = (uint8_t *)msg;
	uint8_t source_buf[18];
	size_t source_len;

	// We received an initiation packet check it is valid
	result = wireguard_check_mac1(device, data,
	    sizeof(struct wireguard_msg_handshake_initiation) -
	    (2 * WIREGUARD_COOKIE_LEN), msg->mac1);
	if (!result) {
		// mac1 is invalid
		return (result);
	}
	if (!wireguard_is_under_load()) {
		// If we aren't under load we only need mac1 to
		// be correct
		result = true;
	} else {
		// If we are under load then check mac2
		source_len = wireguard_iface_get_source_addr_port(wsin->addr,
		    wsin->port, source_buf, sizeof(source_buf));
		result = wireguard_check_mac2(device, data,
		    sizeof(struct wireguard_msg_handshake_initiation) -
		    (WIREGUARD_COOKIE_LEN), source_buf, source_len, msg->mac2);
		if (!result) {
			// mac2 is invalid (cookie may have expired) or
			// not present
			//
			// 5.3 Denial of Service Mitigation & Cookies
			// If the responder receives a message with a valid
			// msg.mac1 yet with an invalid msg.mac2, and is
			// under load, it may respond with a cookie reply
			// message
			wireguard_iface_send_handshake_cookie(device, msg->mac1,
			    msg->sender, wsin);
		}
	}
	return result;
}

static void
wireguard_iface_update_peer_addr(struct wireguard_peer *peer,
    const struct wireguard_sockaddr *wsin)
{

	if (peer->endpoint_latest_ip == wsin->addr &&
	    peer->endpoint_latest_port == wsin->port)
		return;
	peer->endpoint_latest_ip = wsin->addr;
	peer->endpoint_latest_port = wsin->port;
	peer->endpoint_latest_is_proxy = wsin->proxy.from_it;
}

static void
wireguard_iface_send_handshake_response(struct wireguard_device *device,
    struct wireguard_peer *peer)
{
	struct wireguard_msg_handshake_response packet;
	struct pbuf *pbuf = NULL;
	int err = 0;

	if (wireguard_create_handshake_response(device, peer, &packet)) {
		wireguard_start_session(peer, false);
		// Send this packet out!
		pbuf = pbuf_alloc(sizeof(struct wireguard_msg_handshake_response));
		if (pbuf) {
			err = pbuf_take(pbuf, &packet,
			    sizeof(struct wireguard_msg_handshake_response));
			assert(err == 0);
			wireguard_iface_peer_output(device, pbuf, peer, true);
			pbuf_free(pbuf);
		}
	}
}

static bool
wireguard_iface_check_response_message(struct wireguard_device *device,
    struct wireguard_msg_handshake_response *msg,
    struct wireguard_sockaddr *wsin)
{
	bool result;
	uint8_t *data = (uint8_t *)msg;
	uint8_t source_buf[18];
	size_t source_len;

	// We received an initiation packet check it is valid
	result = wireguard_check_mac1(device, data,
	    sizeof(struct wireguard_msg_handshake_response) -
	    (2 * WIREGUARD_COOKIE_LEN), msg->mac1);
	if (result) {
		// mac1 is valid!
		if (!wireguard_is_under_load()) {
			// If we aren't under load we only need mac1 to
			// be correct
			result = true;
		} else {
			// If we are under load then check mac2
			source_len =
			    wireguard_iface_get_source_addr_port(wsin->addr,
				wsin->port, source_buf, sizeof(source_buf));
			result = wireguard_check_mac2(device, data,
			    sizeof(struct wireguard_msg_handshake_response) -
			    WIREGUARD_COOKIE_LEN, source_buf, source_len,
			    msg->mac2);
			if (!result) {
				// mac2 is invalid (cookie may have expired)
				// or not present
				// 5.3 Denial of Service Mitigation & Cookies
				// If the responder receives a message with
				// a valid msg.mac1 yet with an
				// invalid msg.mac2, and is under load,
				// it may respond with a cookie reply message
				wireguard_iface_send_handshake_cookie(device,
				    msg->mac1, msg->sender, wsin);
			}
		}
	} else {
		// mac1 is invalid
	}
	return result;
}

static void
wireguard_iface_process_response_message(struct wireguard_device *device,
    struct wireguard_peer *peer,
    struct wireguard_msg_handshake_response *response,
    struct wireguard_sockaddr *wsin)
{

	if (wireguard_process_handshake_response(device, peer, response)) {
		wireguard_iface_update_peer_addr(peer, wsin);
		wireguard_start_session(peer, true);
		wireguard_iface_send_keepalive(device, peer);
	} else {
		// Packet bad
	}
}

static void
wireguard_iface_tun_write(struct wireguard_device *device, struct pbuf *p)
{

	(void)device;

	wg_stat.n_tun_tx_pkts++;
	wg_stat.bytes_tun_tx += p->tot_len;
	mudband_tunnel_iface_write(p->payload, p->tot_len);
}

static bool
wireguard_iface_apply_acl(struct wireguard_device *device, struct pbuf *pbuf)
{
	struct wireguard_acl *acl = &device->acl;
	size_t i;
	uint32_t r;
	bool need_drop = false;

	for (i = 0; i < acl->n_programs; i++) {
		struct wireguard_acl_program *acl_program;

		acl_program = &acl->programs[i];
		r = mudband_bpf_filter(acl_program->insns, pbuf->payload, pbuf->tot_len,
		    pbuf->tot_len);
		if (r != 0) {
			/* matched */
			if (acl->default_policy == WIREGUARD_ACL_POLICY_ALLOW)
				return (true);
			else if (acl->default_policy == WIREGUARD_ACL_POLICY_BLOCK)
				return (false);
			else
				assert(0 == 1);
		}
	}
	if (acl->default_policy == WIREGUARD_ACL_POLICY_BLOCK)
		need_drop = true;
	return (need_drop);
}

static void
wireguard_iface_process_data_message(struct wireguard_device *device,
    struct wireguard_peer *peer, struct wireguard_msg_transport_data *data_hdr,
    size_t data_len, struct wireguard_sockaddr *wsin)
{
	struct wireguard_keypair *keypair;
	uint64_t nonce;
	uint8_t *src;
	size_t src_len;
	struct pbuf *pbuf;
	struct wireguard_iphdr *iphdr;
	uint32_t dest;
	bool dest_ok = false, r;
	int x;
	uint32_t now;
	uint16_t header_len = 0xFFFF;
	uint32_t idx = data_hdr->receiver;

	(void)device;

	keypair = wireguard_get_peer_keypair_for_idx(peer, idx);
	if (keypair == NULL) {
		// Could not locate valid keypair for remote index
		vtc_log(band_vl, 1, "BANDEC_00080: No keypair found.");
		return;
	}
	if ((keypair->receiving_valid) &&
	    !wireguard_expired(keypair->keypair_millis, WIREGUARD_REJECT_AFTER_TIME) &&
	    (keypair->sending_counter < WIREGUARD_REJECT_AFTER_MESSAGES)) {
		nonce = U8TO64_LITTLE(data_hdr->counter);
		src = &data_hdr->enc_packet[0];
		src_len = data_len;

		// We don't know the unpadded size until we have decrypted
		// the packet and validated/inspected the IP header
		pbuf = pbuf_alloc(src_len - WIREGUARD_AUTHTAG_LEN);
		if (pbuf == NULL) {
			vtc_log(band_vl, 0, "BANDEC_00081: OOM");
			return;
		}
		// Decrypt the packet
		r = wireguard_decrypt_packet(pbuf->payload, src,
		    src_len, nonce, keypair);
		if (r) {
			// 3. Since the packet has authenticated correctly,
			// the source IP of the outer UDP/IP packet is used
			// to update the endpoint for peer TrMv...WXX0.
			// Update the peer location
			wireguard_iface_update_peer_addr(peer, wsin);

			now = wireguard_sys_now();
			keypair->last_rx = now;
			peer->last_rx = now;

			// Might need to shuffle next - key --> current keypair
			wireguard_keypair_update(peer, keypair);

			// Check to see if we should rekey
			if (keypair->initiator &&
			    wireguard_expired(keypair->keypair_millis, WIREGUARD_REJECT_AFTER_TIME - peer->keepalive_interval - WIREGUARD_REKEY_TIMEOUT)) {
				peer->send_handshake = true;
			}
			assert(pbuf->tot_len >= 0);
			if (pbuf->tot_len == 0) {
				// This was a keep-alive packet
				goto drop;
			}

			// 4a. Once the packet payload is decrypted,
			//     the interface has a plaintext packet.
			//     If this is not an IP packet, it is dropped.
			iphdr = (struct wireguard_iphdr *)pbuf->payload;
			// Check for packet replay / dupes
			r = wireguard_check_replay(keypair, nonce);
			if (!r) {
				// This is a duplicate packet / replayed /
				// too far out of order
				goto drop;
			}
			// 4b. Otherwise, WireGuard checks to
			//     see if the source IP address of the plaintext
			//     inner-packet routes correspondingly in
			//     the cryptokey routing table
			//     Also check packet length!
			if (WIREGUARD_IPHDR_HI_BYTE(iphdr->verlen) != 4)
				goto drop;
			dest = iphdr->saddr;
			for (x=0; x < WIREGUARD_MAX_SRC_IPS; x++) {
				uint32_t v1, v2;
				if (!peer->allowed_source_ips[x].valid)
					continue;
				v1 = dest & peer->allowed_source_ips[x].mask;
				v2 = peer->allowed_source_ips[x].ip &
				    peer->allowed_source_ips[x].mask;
				if (v1 == v2) {
					dest_ok = true;
					header_len = ntohs(iphdr->tot_len);
					break;
				}
			}
			if (header_len > pbuf->tot_len) {
				// IP header is corrupt or lied about packet size
				goto drop;
			}
			if (!dest_ok)
				goto drop;
			if (wireguard_iface_apply_acl(device, pbuf))
				goto drop;
			wireguard_iface_tun_write(device, pbuf);
		}
drop:
		if (pbuf)
			pbuf_free(pbuf);
	} else {
		/*
		 * After Reject-After-Messages transport data messages or
		 * after the current secure session is Reject- After-Time
		 * seconds old,
		 * whichever comes first, WireGuard will refuse to send
		 * or receive any more transport data messages using the
		 * current secure session,
		 * until a new secure session is created through
		 * the 1-RTT handshake
		 */
		wireguard_keypair_destroy(keypair);
	}
}

static void
wireguard_iface_network_rx(struct wireguard_device *device, struct pbuf *p,
    struct wireguard_sockaddr *wsin)
 {
	struct wireguard_msg_handshake_initiation *msg_initiation;
	struct wireguard_msg_handshake_response *msg_response;
	struct wireguard_msg_cookie_reply *msg_cookie;
	struct wireguard_msg_transport_data *msg_data;
	struct wireguard_peer *peer;
	size_t len = p->len;
	uint8_t *data = p->payload;
	uint8_t type;
	bool r;

	type = wireguard_get_message_type(data, len);
	switch (type) {
	case WIREGUARD_MSG_HANDSHAKE_INITIATION:
		msg_initiation =
		    (struct wireguard_msg_handshake_initiation *)data;
		/*
		 * Check mac1 (and optionally mac2) are correct
		 * - note it may internally generate a cookie reply packet
		 */
		r = wireguard_iface_check_initiation_message(device,
		    msg_initiation, wsin);
		if (r) {
			peer = wireguard_process_initiation_message(device,
			    msg_initiation);
			if (peer) {
				/* Update the peer location */
				wireguard_iface_update_peer_addr(peer, wsin);
				/* Send back a handshake response */
				wireguard_iface_send_handshake_response(device,
				    peer);
			}
		}
		break;
	case WIREGUARD_MSG_HANDSHAKE_RESPONSE:
		msg_response =
		    (struct wireguard_msg_handshake_response *)data;
		/*
		 * Check mac1 (and optionally mac2) are correct -
		 * note it may internally generate a cookie reply packet
		 */
		r = wireguard_iface_check_response_message(device, msg_response,
		    wsin);
		if (r) {
			peer = wireguard_peer_lookup_by_handshake(device,
			    msg_response->receiver);
			if (peer != NULL) {
				/* Process the handshake response */
				wireguard_iface_process_response_message(device,
				    peer, msg_response, wsin);
			}
		}
		break;
	case WIREGUARD_MSG_COOKIE_REPLY:
		msg_cookie = (struct wireguard_msg_cookie_reply *)data;
		peer = wireguard_peer_lookup_by_handshake(device,
		    msg_cookie->receiver);
		if (peer == NULL)
			break;
		if (wireguard_process_cookie_message(device, peer, msg_cookie)) {
			/* Update the peer location */
			wireguard_iface_update_peer_addr(peer, wsin);
			/*
			 * Don't send anything out
			 * - we stay quiet until the next initiation message
			 */
		}
		break;
	case WIREGUARD_MSG_TRANSPORT_DATA:
		msg_data = (struct wireguard_msg_transport_data *)data;
		peer = wireguard_peer_lookup_by_receiver(device,
		    msg_data->receiver);
		if (peer == NULL)
			break;
		/* header is 16 bytes long so take that off the length */
		wireguard_iface_process_data_message(device, peer, msg_data,
		    len - 16, wsin);
		break;
	default:
		/* Unknown or bad packet header */
		break;
	}
}

static char *
mudband_count2size(uint64_t size, char *result, size_t resultmax)
{   
#define DIM(x) (sizeof(x)/sizeof(*(x)))
	static const char *c_sizes[]   = {
		"EiB", "PiB", "TiB", "GiB", "MiB", "KiB", "B"
	};
	static const uint64_t c_exbibytes = 1024ULL * 1024ULL * 1024ULL *
	    1024ULL * 1024ULL * 1024ULL;
	uint64_t multiplier = c_exbibytes;
	int i;

	for (i = 0; i < DIM(c_sizes); i++, multiplier /= 1024) {
		if (size < multiplier)
			continue;
		if (size % multiplier == 0)
			ODR_snprintf(result, resultmax,
			    "%ju %s", size / multiplier, c_sizes[i]);
		else
			ODR_snprintf(result, resultmax,
			    "%.1f %s", (float) size / multiplier, c_sizes[i]);
		return (result);
	}
	ODR_snprintf(result, resultmax, "0");
	return (result);
#undef DIM
}

static void
wireguard_iface_print_stat(void *arg)
{
	char bytes_tun_rx[20], bytes_tun_tx[20];
	char bytes_udp_rx[20], bytes_udp_tx[20];
	char bytes_udp_proxy_rx[20], bytes_udp_proxy_tx[20];

	(void)arg;

	mudband_count2size(wg_stat.bytes_tun_rx, bytes_tun_rx,
	    sizeof(bytes_tun_rx));
	mudband_count2size(wg_stat.bytes_tun_tx, bytes_tun_tx,
	    sizeof(bytes_tun_tx));
	mudband_count2size(wg_stat.bytes_udp_rx, bytes_udp_rx,
	    sizeof(bytes_udp_rx));
	mudband_count2size(wg_stat.bytes_udp_tx, bytes_udp_tx,
	    sizeof(bytes_udp_tx));
	mudband_count2size(wg_stat.bytes_udp_proxy_rx, bytes_udp_proxy_rx,
	    sizeof(bytes_udp_proxy_rx));
	mudband_count2size(wg_stat.bytes_udp_proxy_tx, bytes_udp_proxy_tx,
	    sizeof(bytes_udp_proxy_tx));

	vtc_log(stats_vl, 2,
	    "n_udp_rx_pkts %ju (%s) n_udp_tx_pkts %ju (%s)"
	    " n_udp_proxy_rx_pkts %ju (%s) n_udp_proxy_tx_pkts %ju (%s)"
	    " n_tun_rx_pkts %ju (%s) n_tun_tx_pkts %ju (%s)"
	    " n_pbuf_cache_count %ju"
	    " n_no_peer_found %ju n_no_ipv4_hdr %ju",
	    wg_stat.n_udp_rx_pkts, bytes_udp_rx,
	    wg_stat.n_udp_tx_pkts, bytes_udp_tx,
	    wg_stat.n_udp_proxy_rx_pkts, bytes_udp_proxy_rx,
	    wg_stat.n_udp_proxy_tx_pkts, bytes_udp_proxy_tx,
	    wg_stat.n_tun_rx_pkts, bytes_tun_rx,
	    wg_stat.n_tun_tx_pkts, bytes_tun_tx,
	    wg_stat.n_pbuf_cache_count,
	    wg_stat.n_no_peer_found, wg_stat.n_no_ipv4_hdr);

	callout_reset(&wg_cb, &wg_stat_co, CALLOUT_SECTOTICKS(300),
	    wireguard_iface_print_stat, NULL);
}

static void
wireguard_iface_peers_update(struct wireguard_device *device, struct cnf *cnf)
{
	struct wireguard_peer *old_peers;
	int i, n_peers, r;
	int peer_index, old_peers_count;
	int n_create = 0, n_reuse = 0, n_failure = 0;

	vtc_log(band_vl, 2, "Updating the wireguard peers information.");

	old_peers_count = device->peers_count;
	old_peers = device->peers;
	n_peers = CNF_get_peer_size(cnf->jroot);
	if (n_peers == 0) {
		device->peers_count = 0;
		device->peers = NULL;
		goto done;
	}
	assert(n_peers > 0);
	device->peers_count = n_peers;
	device->peers = calloc(device->peers_count,
	    sizeof(struct wireguard_peer));
	AN(device->peers);
	for (i = 0; i < n_peers; i++) {
		struct wireguard_iface_peer iface_peer;
		struct wireguard_peer *old_peer, *new_peer;

		wireguard_iface_peer_init(&iface_peer);
		r = CNF_fill_iface_peer(cnf->jroot, &iface_peer, i);
		assert(r == 0);
		old_peer = wireguard_iface_reusable_old_peer(old_peers,
		    old_peers_count, &iface_peer);
		if (old_peer == NULL) {
			r = wireguard_iface_add_peer(device, &iface_peer,
			    &peer_index);
			if (r != 0) {
				vtc_log(band_vl, 0,
				    "BANDEC_00082: wireguard_iface_add_peer()"
				    " failed: r %d", r);
				n_failure++;
				continue;
			}
			assert(peer_index != WIREGUARD_IFACE_INVALID_INDEX);
			r = wireguard_iface_connect(device, peer_index);
			if (r != 0) {
				vtc_log(band_vl, 0,
				    "BANDEC_00083: wireguard_iface_connect()"
				    " failed: r %d", r);
				n_failure++;
				continue;
			}
			n_create++;
		} else {
			new_peer = wireguard_peer_alloc(device);
			AN(new_peer);
			*new_peer = *old_peer;
			n_reuse++;
		}
	}
done:
	if (old_peers != NULL)
		free(old_peers);
	vtc_log(band_vl, 2,
	    "Completed to update the wireguard peers information."
	    " (%d peers %d create %d reuse %d failure)",
	    n_peers, n_create, n_reuse, n_failure);
}

static void
wireguard_iface_bpf_update(struct wireguard_device *device, struct cnf *cnf)
{
	struct wireguard_acl *acl;

	acl = CNF_acl_build(cnf->jroot);
	if (acl == NULL)
		return;
	device->acl = *acl;
	free(acl);
}

static void
wireguard_iface_sync(struct wireguard_device *device)
{
	struct cnf *cnf;

	CNF_get(&cnf);
	wireguard_iface_peers_update(device, cnf);
	wireguard_iface_bpf_update(device, cnf);
	CNF_rel(&cnf);
}

void
VAS_Fail(const char *func, const char *file, int line, const char *cond,
	    int xxx)
{

	fprintf(stdout, "Critical! assert fail: %s %s:%d %s %d\n", func, file,
	    line, cond, xxx);
	abort();
}

static void
mudband_init(void)
{
	int r;
	const char *cdir;
	char edir[ODR_BUFSIZ];

	ODR_libinit();
	vtc_loginit();
	VHTTPS_init();
	mudband_signal_init();
	band_vl = vtc_logopen("band", NULL);
	AN(band_vl);
	stats_vl = vtc_logopen("stats", NULL);
	AN(stats_vl);
	r = ODR_corefile_init();
	if (r != 0) {
		vtc_log(band_vl, 1,
		    "Failed to initialize the corefile handler: %d %s",
		    ODR_errno(), ODR_strerror(ODR_errno()));
	}
	cdir = ODR_confdir();
	ODR_mkdir_recursive(cdir);
	band_confdir_root = ODR_strdup(cdir);
	ODR_snprintf(edir, sizeof(edir), "%s/enroll", band_confdir_root);
	ODR_mkdir_recursive(edir);
	band_confdir_enroll = ODR_strdup(edir);
	PBUF_init();
	MBE_init();
	MCM_init();
	CNF_init();
	MPC_init();
	ACL_init();
	MWC_init();
}

static void
print_version(void)
{

	fprintf(stdout, "mudband %s\n", MUDBAND_VERSION);
	exit(0);
}

static int
mudband_tunnel_init_chkconfig(void)
{
	int r;

	/* load if there's a previous one before fetching the config. */
	(void)CNF_check_and_read();
	r = CNF_fetch("when_it_runs_first");
	if (r < 0) {
		vtc_log(band_vl, 1,
		    "BANDEC_00084: Failed to fetch the config."
		    " Try to use the cache if it exists");
	}
	r = CNF_check_and_read();
	switch (r) {
	case -3:	/* nat type changed */
	case -4:	/* mapped address changed */
	case -5:	/* no peers exist */
	case 0:
		break;
	default:
		vtc_log(band_vl, 2,
		    "BANDEC_00085: Failed to read the config"
		    " from the disk.");
		return (-1);
	}
	return (0);
}

static int
mudband_tunnel_init(void)
{
	int r;

	r = MBE_check_and_read();
	if (r == -1) {
		vtc_log(band_vl, 0, "BANDEC_00086: Enrollment check failed.");
		return (1);
	}
	STUNC_init();
	MQTT_init();
	MBT_init();
	r = mudband_tunnel_init_chkconfig();
	if (r != 0)
		return (1);
	MQTT_subscribe();
	return (0);
}

static void
mudband_tunnel_fini(void)
{

	CNF_fini();
	MBE_fini();
	MBT_fini();
}

static int
mudband_tunnel_proxy_handler(struct pbuf *p, struct wireguard_sockaddr *wsin)
{
	struct wireguard_proxy_pkthdr *pkthdr;
	vuuid_t band_uuid;
	int r;

	assert(p->len > sizeof(*pkthdr));
	pkthdr = (struct wireguard_proxy_pkthdr *)p->payload;
	if (pkthdr->f_version != 1) {
		wg_stat.n_udp_proxy_rx_errs++;
		return (-1);
	}
	memcpy(&band_uuid, pkthdr->band_uuid,
	    sizeof(band_uuid));
	r = VUUID_compare(&band_uuid, MBE_get_uuid());
	if (r != 0) {
		wg_stat.n_udp_proxy_rx_errs++;
		return (-1);
	}
	wsin->proxy.src_addr = pkthdr->src_addr;
	wsin->proxy.dst_addr = pkthdr->dst_addr;
	p->payload += sizeof(*pkthdr);
	p->len -= sizeof(*pkthdr);
	wg_stat.n_udp_proxy_rx_pkts++;
	wg_stat.bytes_udp_proxy_rx += p->len;
	return (0);
}

static int
mudband_tunnel(void)
{
	struct cnf *cnf;
	struct timeval tv;
	struct wireguard_device *device;
	struct wireguard_iface_init_data init_data;
	fd_set rset;
	ssize_t len;
	int fd, r, mtu;
	const char *private_ip, *private_mask;

	r = mudband_tunnel_init();
	if (r != 0)
		return (r);

	COT_init(&wg_cb);
	callout_init(&wg_stat_co, 0);
	callout_reset(&wg_cb, &wg_stat_co, CALLOUT_SECTOTICKS(60),
	    wireguard_iface_print_stat, NULL);
	wireguard_init();

	CNF_get(&cnf);
	private_ip = CNF_get_interface_private_ip(cnf->jroot);
	AN(private_ip);
	private_mask = CNF_get_interface_private_mask(cnf->jroot);
	AN(private_mask);
	mtu = CNF_get_interface_mtu(cnf->jroot);
	assert(mtu > 0);
	fd = mudband_tunnel_iface_init(private_ip, private_mask, mtu);
	if (fd == -1) {
		vtc_log(band_vl, 0,
		    "BANDEC_00453: mudband_tunnel_iface_init() failed. Exit.");
		return (1);
	}

	vtc_log(band_vl, 2, "My private IP is %s", private_ip);

	init_data.private_ip = private_ip;
	init_data.private_key = MBE_get_private_key();
	init_data.listen_fd = CNF_get_interface_listen_fd();
	device = wireguard_iface_init(&init_data);
	AN(device);
	assert(device->udp_fd >= 0);
	CNF_rel(&cnf);

	while (!wg_aborted) {
		struct wireguard_iphdr *iphdr;
		int maxfd;

		if (band_need_iface_sync) {
			band_need_iface_sync = 0;
			wireguard_iface_sync(device);
		}
		tv.tv_sec = 0;
		tv.tv_usec = 300000;
		FD_ZERO(&rset);
		FD_SET(fd, &rset);
		maxfd = fd;
		FD_SET(device->udp_fd, &rset);
		maxfd = MAX(maxfd, device->udp_fd);
		r = select(maxfd + 1, &rset, NULL, NULL, &tv);
		if (r == -1) {
			if (errno == EINTR)
				goto done;
			TODO();
		}
		if (r == 0)
			goto done;
		if (FD_ISSET(fd, &rset)) {
			struct pbuf *p;

			p = pbuf_alloc(2048);
			AN(p);
			len = ODR_read(band_vl, fd, p->payload, p->tot_len);
			assert(len >= 0);
			p->len = (size_t)len;
			iphdr = (struct wireguard_iphdr *)p->payload;
			if (WIREGUARD_IPHDR_HI_BYTE(iphdr->verlen) != 4) {
				wg_stat.n_no_ipv4_hdr++;
				pbuf_free(p);
				goto next;
			}
			wg_stat.n_tun_rx_pkts++;
			wg_stat.bytes_tun_rx += p->len;
			wireguard_iface_output(device, p, iphdr->daddr);
			pbuf_free(p);
		}
next:
		if (FD_ISSET(device->udp_fd, &rset)) {
			struct pbuf *p;
			struct sockaddr_in sin;
			struct wireguard_sockaddr wsin;
			socklen_t sinlen;
			bool from_proxy = false;

			sinlen = sizeof(sin);
			p = pbuf_alloc(2048);
			AN(p);
			len = recvfrom(device->udp_fd, p->payload,
			    p->tot_len, 0, (struct sockaddr *)&sin,
			    (socklen_t*)&sinlen);
			assert(len >= 0);
			p->len = (size_t)len;
			wg_stat.n_udp_rx_pkts++;
			wg_stat.bytes_udp_rx += p->len;
			if (ntohs(sin.sin_port) == 82 /* proxy port */) {
				from_proxy = true;
				r = mudband_tunnel_proxy_handler(p, &wsin);
				if (r != 0) {
					pbuf_free(p);
					goto done;
				}
			}
			wsin.addr = sin.sin_addr.s_addr;
			wsin.port = ntohs(sin.sin_port);
			wsin.proxy.from_it = from_proxy;
			wireguard_iface_network_rx(device, p, &wsin);
			pbuf_free(p);
		}
done:
		COT_ticks(&wg_cb);
		COT_clock(&wg_cb);
	}

	wireguard_iface_print_stat(NULL);
	wireguard_iface_fini(device);
	COT_fini(&wg_cb);

	mudband_tunnel_fini();

	vtc_log(band_vl, 2, "Exited.");

	return (0);
}

static void
usage(void)
{
#define FMT "    %-28s # %s\n"
#define FMT_LONG "    %-28s\n"

	fprintf(stderr, "Usage: %s [options]\n", orig_argv[0]);
	fprintf(stderr, FMT, "--acl-add <syntax>",
	    "Add an ACL rule.");
	fprintf(stderr, FMT, "--acl-default-policy allow|block",
	    "Change the default ACL policy.");
	fprintf(stderr, FMT, "--acl-del <acl_id>",
	    "Delete an ACL rule.");
	fprintf(stderr, FMT, "--acl-priority <priority>",
	    "Specify the priority of an ACL rule.");
	fprintf(stderr, FMT, "--acl-list", "Get the ACL list.");
	fprintf(stderr, FMT, "-b <uuid>", "Specify the band UUID to use.");
	fprintf(stderr, FMT_LONG, "--band-uuid <uuid>");
	fprintf(stderr, FMT, "-e <token>", "Enroll with the given token.");
	fprintf(stderr, FMT_LONG, "--enroll-token <token>");
	fprintf(stderr, FMT, "--enroll-list",
	    "List the enrollments in the disk.");
	fprintf(stderr, FMT, "--enroll-secret <secret>",
	    "Set the secret for the enrollment.");
	fprintf(stderr, FMT, "-h, --help", "Print this message and exit.");
	fprintf(stderr, FMT, "-n <device_name>",
	    "Specify the device name.");
	fprintf(stderr, FMT_LONG, "--name <device_name>");
	fprintf(stderr, FMT, "-v", "Print the version.");
	fprintf(stderr, FMT, "-W, --webcli", "Get a URL to access WebCLI.");
	exit(1);
#undef FMT
}

int
main(int argc, char *argv[])
{
	struct vopt_option opts[] = {
		{ "acl-add", vopt_long_required_argument, NULL, '!' },
		{ "acl-default-policy", vopt_long_required_argument, NULL, '@' },
		{ "acl-del", vopt_long_required_argument, NULL, '$' },
		{ "acl-list", vopt_long_no_argument, NULL, '#' },
		{ "acl-priority", vopt_long_required_argument, NULL, '%' },
		{ "band-uuid", vopt_long_required_argument, NULL, 'b' },
		{ "daemon", vopt_long_no_argument, NULL, 'D' },
		{ "device-name", vopt_long_required_argument, NULL, 'n' },
		{ "enroll-list", vopt_long_no_argument, NULL, '&' },
		{ "enroll-secret", vopt_long_required_argument, NULL, '^' },
		{ "enroll-token", vopt_long_required_argument, NULL, 'e' },
		{ "help", vopt_long_no_argument, NULL, 'h' },
		{ "webcli", vopt_long_no_argument, NULL, 'W' },
		{ NULL, 0, NULL, 0 }
	};
	unsigned acl_list_flag = 0;
	unsigned enroll_list_flag = 0;
	unsigned W_flag = 0;
	int ch;
	const char *acl_add_arg = NULL;
	const char *acl_default_policy_arg = NULL;
	const char *acl_del_arg = NULL;
	const char *acl_priority_arg = NULL;
	const char *e_arg = NULL;
	const char *enroll_secret_arg = "";
	const char *n_arg = NULL;
	unsigned D_flag = 0;
	const char *opt = "b:e:hn:vW";

	orig_argc = argc;
	orig_argv = argv;
	while ((ch = VOPT_get_long(argc, argv, opt, opts, NULL)) != -1)
		switch (ch) {
		case '!': /* acl-add */
			acl_add_arg = vopt_arg;
			break;
		case '@': /* acl-default-policy */
			acl_default_policy_arg = vopt_arg;
			break;
		case '$': /* acl-del */
			acl_del_arg = vopt_arg;
			break;
			break;
		case '#': /* acl-list */
			acl_list_flag = 1 - acl_list_flag;
			break;
		case '%':
			acl_priority_arg = vopt_arg;
			break;
		case '^':
			enroll_secret_arg = vopt_arg;
			break;
		case '&':
			enroll_list_flag = 1 - enroll_list_flag;
			break;
		case 'b':
			band_b_arg = vopt_arg;
			break;
		case 'e':
			e_arg = vopt_arg;
			break;
		case 'n':
			n_arg = vopt_arg;
			break;
		case 'v':
			print_version();
			/* NOTREACHED */
		case 'W':
			W_flag = 1 - W_flag;
			break;
		case 'h':
		default:
			usage();
			/* NOTREACHED */
		}
	argv += vopt_ind;
	argc -= vopt_ind;

	mudband_init();

	if (e_arg != NULL)
		return (MBE_enroll(e_arg, n_arg, enroll_secret_arg));
	if (enroll_list_flag)
		return (MBE_list());
	if (acl_add_arg != NULL || acl_del_arg != NULL ||
	    acl_default_policy_arg != NULL || acl_list_flag)
		return (ACL_cmd(acl_add_arg, acl_priority_arg, acl_list_flag,
		    acl_del_arg, acl_default_policy_arg));
	if (W_flag)
		return (MWC_get());
	return (mudband_tunnel());
}
