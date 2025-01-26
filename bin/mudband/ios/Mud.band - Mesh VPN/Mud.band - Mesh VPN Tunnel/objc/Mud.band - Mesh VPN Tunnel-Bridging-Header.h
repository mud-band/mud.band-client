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

#ifndef MUDBAND_TUNNEL_BRIDGING_H
#define MUDBAND_TUNNEL_BRIDGING_H

#import <Foundation/Foundation.h>

/* mudband_tunnel.m */
int     mudband_tunnel_init(NSString *top_dir, NSString *enroll_dir);
void    mudband_tunnel_log(int level, NSString *msg);

/* mudband_tunnel_confmgr.m */
NSString *
        mudband_tunnel_confmgr_get_etag(void);
NSMutableArray *
        mudband_tunnel_confmgr_getifaddrs(void);
int     mudband_tunnel_confmgr_parse_response(NSString *etag, NSString *resp);
NSString *
        mudband_tunnel_confmgr_get_interface_private_ip(void);
NSString *
        mudband_tunnel_confmgr_get_interface_private_mask(void);
int     mudband_tunnel_confmgr_get_interface_mtu(void);

/* mudband_tunnel_connmgr.m */
int     mudband_tunnel_connmgr_listen_port(void);

/* mudband_tunnel_enroll.m */
NSString *
        mudband_tunnel_enroll_get_jwt(void);

/* mudband_tunnel_stun_client.m */
int     mudband_tunnel_stun_client_get_nattype_int(void);
NSString *
        mudband_tunnel_stun_client_get_mappped_addr(void);

/* mudband_tunnel_wireguard.m */
int     mudband_tunnel_wireguard_init(void);
void    mudband_tunnel_wireguard_ticks(void);
int     mudband_tunnel_wireguard_rx_listen(void);
int     mudband_tunnel_wireguard_rx_recvfrom(void);
void    mudband_tunnel_wireguard_tx(NSData *data);
@class PacketTunnelProvider;
void    mudband_tunnel_iface_set_tunnel_provider(PacketTunnelProvider *obj);
void    mudband_tunnel_iface_need_peers_update(void);

#endif /* MUDBAND_TUNNEL_BRIDGING_H */
