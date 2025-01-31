/*-
 * Copyright (c) 2011-2014 Weongyo Jeong <weongyo@gmail.com>
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

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "crypto.h"
#include "libwireguard.h"
#include "x25519.h"

#define wireguard_x25519(a,b,c)     x25519(a,b,c,1)

static const uint8_t zero_key[WIREGUARD_PUBLIC_KEY_LEN] = { 0 };
static const char *base64_lookup =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void
wireguard_random_bytes(void *bytes, size_t size)
{
    int x;
    uint8_t *out = (uint8_t *)bytes;

    for (x = 0; x < (int)size; x++) {
        out[x] = rand() % 0xFF;
    }
}

static void
wireguard_clamp_private_key(uint8_t *key)
{
    
    key[0] &= 248;
    key[31] = (key[31] & 127) | 64;
}

void
wireguard_generate_private_key(uint8_t *key)
{

    wireguard_random_bytes(key, WIREGUARD_PRIVATE_KEY_LEN);
    wireguard_clamp_private_key(key);
}

bool
wireguard_generate_public_key(uint8_t *public_key, const uint8_t *private_key)
{
    static const uint8_t basepoint[WIREGUARD_PUBLIC_KEY_LEN] = { 9 };
    bool r = false;

    if (memcmp(private_key, zero_key, WIREGUARD_PUBLIC_KEY_LEN) != 0) {
        r = (wireguard_x25519(public_key, private_key, basepoint) == 0);
    }
    return r;
}

bool
wireguard_base64_encode(const uint8_t *in, size_t inlen, char *out,
    size_t *outlen)
{
    bool result = false;
    int read_offset = 0;
    int write_offset = 0;
    uint8_t byte1, byte2, byte3;
    uint32_t tmp;
    char c;
    size_t len = 4 * ((inlen + 2) / 3);
    int padding = (3 - (inlen % 3));

    if (padding > 2)
        padding = 0;
    if (*outlen > len) {
        while (read_offset < (int)inlen) {
            // Read three bytes
            byte1 = (read_offset < (int)inlen) ? in[read_offset++] : 0;
            byte2 = (read_offset < (int)inlen) ? in[read_offset++] : 0;
            byte3 = (read_offset < (int)inlen) ? in[read_offset++] : 0;
            // Turn into 24 bit intermediate
            tmp = (byte1 << 16) | (byte2 << 8) | (byte3);
            // Write out 4 characters each representing
            // 6 bits of input
            out[write_offset++] = base64_lookup[(tmp >> 18) & 0x3F];
            out[write_offset++] = base64_lookup[(tmp >> 12) & 0x3F];
            c = (write_offset < (int)(len - padding)) ?
                base64_lookup[(tmp >> 6) & 0x3F] : '=';
            out[write_offset++] = c;
            c = (write_offset < (int)(len - padding)) ?
                base64_lookup[(tmp) & 0x3F] : '=';
            out[write_offset++] = c;
        }
        out[len] = '\0';
        *outlen = len;
        result = true;
    } else {
        // Not enough data to put in base64 and null terminate
    }
    return result;
}
