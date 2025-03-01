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

#ifndef MUD_BAND_MUDBAND_WIREGUARD_H
#define MUD_BAND_MUDBAND_WIREGUARD_H

#include <stdbool.h>
#include <stdint.h>

#define WIREGUARD_IFACE_DEFAULT_PORT		(51820)
#define WIREGUARD_IFACE_KEEPALIVE_DEFAULT	(0xFFFF)
#define WIREGUARD_IFACE_INVALID_INDEX		(-1)

#define WIREGUARD_IPHDR_HI_BYTE(byte)	    (((byte) >> 4) & 0x0F)
#define WIREGUARD_IPHDR_LO_BYTE(byte)	    ((byte) & 0x0F)

/* mudband_wireguard.c */
struct wireguard_iface_init_data {
    /* Required: the private key of this WireGuard network interface */
    const char *private_key;
    /* Required: What UDP port to listen on */
    int listen_fd;
    const char *private_ip;
};

#define	WIREGUARD_IFACE_PEER_ENDPOINTS_MAX	16

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

struct wireguard_iface_stat {
    uint64_t	n_no_peer_found;
    uint64_t	n_no_ipv4_hdr;
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
extern struct wireguard_iface_stat wg_iface_stat;

struct wireguard_sockaddr {
    uint32_t	addr;
    uint16_t	port;
    struct {
        bool		from_it;
        uint32_t	src_addr;
        uint32_t	dst_addr;
    } proxy;
};

struct wireguard_device *
        wireguard_iface_init(struct vtclog *vl, struct wireguard_iface_init_data *init_data);
void    wireguard_iface_sync(struct wireguard_device *device);
struct pbuf;
int     wireguard_iface_output(struct wireguard_device *device, struct pbuf *p, const uint32_t ipaddr);
void    wireguard_iface_network_rx(struct wireguard_device *device, struct pbuf *p, struct wireguard_sockaddr *wsin);

void    MWG_init(void);

#endif //MUD_BAND_MUDBAND_WIREGUARD_H
