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
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "odr.h"
#include "vassert.h"
#include "vqueue.h"
#include "vtc_log.h"

#include "crypto.h"
#include "wireguard.h"
#include "wireguard-pbuf.h"

#include "mudband.h"
#include "mudband_wireguard.h"

#define TODO()		do { assert(0 == 1); } while (0)

struct wireguard_iface_stat wg_iface_stat;
static struct vtclog *mwg_vl;

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
            buf = mudband_tunnel_proxy_prepend_pkthdr(buf, &buflen,
                                                      device->iface_addr,
						      pr->iface_addr);
            wg_iface_stat.n_udp_proxy_tx_pkts++;
            wg_iface_stat.bytes_udp_proxy_tx += buflen;
        }
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = pr->endpoints[x].ip;
        sin.sin_port = htons(pr->endpoints[x].port);
        l = sendto(device->udp_fd, buf, buflen, 0,
                   (struct sockaddr *)&sin, sizeof(sin));
        if (l == -1) {
            vtc_log(mwg_vl, 0,
                    "BANDEC_00198: sendto(2) to %s:%d failed: %d %s",
                    inet_ntoa(sin.sin_addr), pr->endpoints[x].port,
                    errno, strerror(errno));
            return (-1);
        }
        assert(l == buflen);
        wg_iface_stat.n_udp_tx_pkts++;
        wg_iface_stat.bytes_udp_tx += buflen;
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
        buf = mudband_tunnel_proxy_prepend_pkthdr(buf, &buflen,
                                                  device->iface_addr, peer->iface_addr);
        wg_iface_stat.n_udp_proxy_tx_pkts++;
        wg_iface_stat.bytes_udp_proxy_tx += buflen;
    }
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = peer->endpoint_latest_ip;
    sin.sin_port = htons(peer->endpoint_latest_port);
    l = sendto(device->udp_fd, buf, buflen, 0,
               (struct sockaddr *)&sin, sizeof(sin));
    if (l == -1) {
        vtc_log(mwg_vl, 0,
		"BANDEC_00817: sendto(2) to %s:%d failed: %d %s",
		inet_ntoa(sin.sin_addr), ntohs(sin.sin_port),
		errno, strerror(errno));
        return (-1);
    }
    assert(l == buflen);
    wg_iface_stat.n_udp_tx_pkts++;
    wg_iface_stat.bytes_udp_tx += buflen;
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

    callout_reset(&device->cb, &device->co, CALLOUT_MSTOTICKS(400),
                  wireguard_iface_timer, device);
}

struct wireguard_device *
wireguard_iface_init(struct vtclog *vl, struct wireguard_iface_init_data *init_data)
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
    COT_init(&device->cb);
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

    vtc_log(vl, 2, "Initialized the wireguard device.");

    callout_reset(&device->cb, &device->co, CALLOUT_MSTOTICKS(400),
                  wireguard_iface_timer, device);
    return (device);
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

static int
wireguard_iface_otp_reusable(struct wireguard_peer *peer,
                             struct wireguard_iface_peer *p)
{
    int i;

    if (peer->otp_enabled != p->otp_enabled)
        return (0);
    if (peer->otp_sender != p->otp_sender)
        return (0);
    for (i = 0; i < 3; i++) {
        if (peer->otp_receiver[i] != p->otp_receiver[i])
            return (0);
    }
    return (1);
}

static void
wireguard_iface_otp_update(struct wireguard_peer *peer,
                           struct wireguard_iface_peer *p)
{
    int i;

    peer->otp_enabled = p->otp_enabled;
    peer->otp_sender = p->otp_sender;
    for (i = 0; i < 3; i++)
        peer->otp_receiver[i] = p->otp_receiver[i];
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
    if (!wireguard_iface_otp_reusable(peer, p))
        return (NULL);
    return (peer);
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
        vtc_log(mwg_vl, 0, "BANDEC_00199: Invalid public key %s",
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
        vtc_log(mwg_vl, 0, "BANDEC_00200: No room for new peer");
        return (-1);
    }
    r = wireguard_peer_init(device, peer, public_key, p->preshared_key);
    if (!r) {
      vtc_log(mwg_vl, 0,
              "BANDEC_00201: wireguard_peer_init() failed");
        return (-1);
    }
    wireguard_iface_otp_update(peer, p);
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
        vtc_log(mwg_vl, 2,
                "Added a peer (private_ip %s idx %d n_endpoints %d opt %#jx)",
                inet_ntoa(in), *peer_index, peer->n_endpoints,
                peer->otp_sender);
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
        vtc_log(mwg_vl, 0,
                "BANDEC_00202: wireguard_iface_lookup_peer() failed");
        return (-1);
    }
    /* Check that a valid connect ip and port have been set */
    if (peer->endpoints[0].ip == INADDR_ANY || peer->endpoints[0].port == 0) {
        vtc_log(mwg_vl, 0,
                "BANDEC_00203: Invalid endpoint ip/port for peer");
        return (-1);
    }
    /* Set the flag that we want to try connecting */
    peer->active = true;
    peer->endpoint_latest_ip = peer->endpoints[0].ip;
    peer->endpoint_latest_port = peer->endpoints[0].port;
    peer->endpoint_latest_is_proxy = peer->endpoints[0].is_proxy;
    return (0);
}

void
wireguard_iface_peers_update(struct wireguard_device *device, struct cnf *cnf)
{
    struct wireguard_peer *old_peers;
    int i, n_peers, r;
    int peer_index, old_peers_count;
    int n_create = 0, n_reuse = 0, n_failure = 0;

    vtc_log(mwg_vl, 2, "Updating the wireguard peers information.");

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
    device->peers = calloc(device->peers_count, sizeof(struct wireguard_peer));
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
                vtc_log(mwg_vl, 0,
                        "BANDEC_00204: wireguard_iface_add_peer()"
                        " failed: r %d", r);
                n_failure++;
                continue;
            }
            assert(peer_index != WIREGUARD_IFACE_INVALID_INDEX);
            r = wireguard_iface_connect(device, peer_index);
            if (r != 0) {
                vtc_log(mwg_vl, 0,
                        "BANDEC_00205: wireguard_iface_connect()"
                        " failed: r %d", r);
                n_failure++;
                continue;
            }
            n_create++;
        } else {
            new_peer = wireguard_peer_alloc(device);
            AN(new_peer);
            *new_peer = *old_peer;
            wireguard_iface_otp_update(new_peer, &iface_peer);
            n_reuse++;
        }
    }
    done:
    if (old_peers != NULL)
        free(old_peers);
    vtc_log(mwg_vl, 2,
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

void
wireguard_iface_sync(struct wireguard_device *device)
{
    struct cnf *cnf;

    CNF_get(&cnf);
    wireguard_iface_peers_update(device, cnf);
    wireguard_iface_bpf_update(device, cnf);
    CNF_rel(&cnf);
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

int
wireguard_iface_output(struct wireguard_device *device, struct pbuf *p,
                       const uint32_t ipaddr)
{
    struct wireguard_peer *peer;

    peer = wireguard_iface_peer_lookup_by_allowed_ip(device, ipaddr);
    if (peer == NULL) {
        /* No peer found - drop packet */
        wg_iface_stat.n_no_peer_found++;
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

static int
wireguard_iface_device_output(struct wireguard_device *device, struct pbuf *q,
                              struct wireguard_sockaddr *wsin)
{
    struct sockaddr_in sin;
    size_t buflen = q->len;
    ssize_t l;
    uint8_t *buf = q->payload;

    if (wsin->proxy.from_it) {
        buf = mudband_tunnel_proxy_prepend_pkthdr(buf, &buflen,
                                                  device->iface_addr, wsin->proxy.src_addr);
        wg_iface_stat.n_udp_proxy_tx_pkts++;
        wg_iface_stat.bytes_udp_proxy_tx += buflen;
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
    wg_iface_stat.n_udp_tx_pkts++;
    wg_iface_stat.bytes_udp_tx += buflen;
    return (0);
}

static void
wireguard_iface_send_handshake_cookie(struct wireguard_device *device,
                                      const uint8_t *mac1, uint32_t index,
                                      struct wireguard_sockaddr *wsin)
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

bool
wireguard_is_under_load(void)
{

    return (false);
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
                                                          wsin->port, source_buf,
                                                          sizeof(source_buf));
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
    peer->endpoint_latest_t_heartbeated = time(NULL);
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
                                                         wsin->port, source_buf,
                                                         sizeof(source_buf));
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

    wg_iface_stat.n_tun_tx_pkts++;
    wg_iface_stat.bytes_tun_tx += p->tot_len;
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
        vtc_log(mwg_vl, 1, "BANDEC_00206: No keypair found.");
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
            vtc_log(mwg_vl, 0, "BANDEC_00207: OOM");
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

void
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

void
MWG_init(void)
{

    mwg_vl = vtc_logopen("wg", mudband_log_printf);
    AN(mwg_vl);
}
