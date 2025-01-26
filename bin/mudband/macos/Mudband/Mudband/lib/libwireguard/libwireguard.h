//
//  libwireguard.h
//  Mudband
//
//  Created by Weongyo Jeong on 12/10/24.
//

#ifndef libwireguard_h
#define libwireguard_h

#include <stdbool.h>

/* Public key algo is curve22519 which uses 32 byte keys */
#define WIREGUARD_PUBLIC_KEY_LEN    (32)
/* Public key algo is curve22519 which uses 32 byte keys */
#define WIREGUARD_PRIVATE_KEY_LEN   (32)

void    wireguard_generate_private_key(uint8_t *key);
bool    wireguard_generate_public_key(uint8_t *public_key, const uint8_t *private_key);
bool    wireguard_base64_encode(const uint8_t *in, size_t inlen, char *out,
                                size_t *outlen);

#endif /* libwireguard_h */
