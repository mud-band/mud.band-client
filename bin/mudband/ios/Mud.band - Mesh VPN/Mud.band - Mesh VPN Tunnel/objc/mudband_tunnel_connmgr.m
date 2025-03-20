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

#include <netinet/in.h>

#include "odr.h"
#include "odr_pthread.h"
#include "vassert.h"
#include "vqueue.h"
#include "vsock.h"
#include "vtc_log.h"

#include "mudband_tunnel.h"

static struct vtclog *connmgr_vl;
static int connmgr_listen_port = -1;
static int connmgr_listen_fd = -1;
static char connmgr_listen_addrstr[VSOCK_ADDRBUFSIZE];
static char connmgr_listen_portstr[VSOCK_PORTBUFSIZE];

static int
mudband_tunnel_connmgr_open_port(uint16_t port)
{
    struct sockaddr_in addr;
    int fd;
    
    fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == -1)
        return (-1);
    memset((char *)&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return (-1);
    }
    assert(fd >= 0);
    return (fd);
}

int
mudband_tunnel_connmgr_listen_port(void)
{
    struct mudband_tunnel_bandconf *cnf;
    int listen_port = -1, r;

    if (connmgr_listen_port == -1) {
        r = mudband_tunnel_confmgr_get(&cnf);
        if (r == 0) {
            listen_port = mudband_tunnel_confmgr_get_interface_listen_port(cnf->jroot);
            mudband_tunnel_confmgr_rel(&cnf);
        }
        if (listen_port == -1) {
            connmgr_listen_fd = mudband_tunnel_connmgr_open_port(0);
        } else {
            connmgr_listen_fd = mudband_tunnel_connmgr_open_port(listen_port);
	    if (connmgr_listen_fd < 0) {
	        vtc_log(connmgr_vl, 1,
			"BANDEC_XXXXX: mudband_tunnel_connmgr_open_port(%d)"
			" failed. Retrying to open any port.",
			listen_port);
		connmgr_listen_fd = mudband_tunnel_connmgr_open_port(0);
	    }
	}
        assert(connmgr_listen_fd >= 0);
        VSOCK_myname(connmgr_listen_fd,
                     connmgr_listen_addrstr, sizeof(connmgr_listen_addrstr),
                     connmgr_listen_portstr, sizeof(connmgr_listen_portstr));
        connmgr_listen_port = atoi(connmgr_listen_portstr);
        vtc_log(connmgr_vl, 2, "Listening on UDP %s:%s", connmgr_listen_addrstr,
                connmgr_listen_portstr);
    }
    return (connmgr_listen_port);
}

int
mudband_tunnel_connmgr_listen_fd(void)
{

    (void)mudband_tunnel_connmgr_listen_port();
    assert(connmgr_listen_fd >= 0);
    return (connmgr_listen_fd);
}

int
mudband_tunnel_connmgr_init(void)
{
    
    connmgr_vl = vtc_logopen("connmgr", mudband_tunnel_log_callback);
    AN(connmgr_vl);
    return (0);
}
