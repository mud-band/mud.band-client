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

#ifndef MUDBAND_STUN_CLIENT_H
#define MUDBAND_STUN_CLIENT_H

enum stun_nattype {
    STUN_NATTYPE_UNKNOWN = 0,
    STUN_NATTYPE_FAILURE = 1,
    STUN_NATTYPE_OPEN = 2,
    STUN_NATTYPE_BLOCKED = 3,
    STUN_NATTYPE_FULL_CONE = 4,
    STUN_NATTYPE_RESTRICTED_CONE = 5,
    STUN_NATTYPE_PORT_RESTRICTED_CONE = 6,
    STUN_NATTYPE_SYMMETRIC = 7,
    STUN_NATTYPE_FIREWALL = 8,
};

struct stun_client_result {
    enum stun_nattype   nattype;
    uint32_t            mapped_addr;
};

int     mudband_tunnel_stun_client_init(void);
int     mudband_tunnel_stun_client_test(void);
enum stun_nattype
        mudband_tunnel_stun_client_get_nattype(void);
NSString *
        mudband_tunnel_stun_client_get_mappped_addr(void);
const char *
        mudband_tunnel_stun_client_nattypestr(enum stun_nattype t);

#endif /* MUDBAND_STUN_CLIENT_H */
