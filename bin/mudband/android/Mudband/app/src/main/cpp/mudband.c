/*-
 * Copyright (c) 2024 Weongyo Jeong <weongyo@gmail.com>
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

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <jni.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <android/log.h>

#include "odr.h"
#include "vassert.h"
#include "vqueue.h"
#include "vsb.h"
#include "vtc_log.h"
#include "vtim.h"

#include "callout.h"
#include "crypto.h"
#include "wireguard.h"
#include "wireguard-pbuf.h"

#include "mudband.h"
#include "mudband_mqtt.h"
#include "mudband_stun_client.h"
#include "mudband_wireguard.h"

#define TODO()		do { assert(0 == 1); } while (0)

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

static pthread_mutex_t  band_mtx = PTHREAD_MUTEX_INITIALIZER;
static int              band_inited;
char                    band_root_dir[256];
char                    band_enroll_dir[256];
static struct vtclog    *band_vl;
static struct vtclog    *band_ui_vl;
static struct wireguard_device *band_device;
static int              band_tun_fd = -1;
int                     band_need_iface_sync;
int                     band_need_fetch_config;

void
VAS_Fail(const char *func, const char *file, int line, const char *cond,
         int xxx)
{

    __android_log_print(ANDROID_LOG_FATAL, "Mud.band",
                        "Critical! assert fail: %s %s:%d %s %d\n", func, file,
                        line, cond, xxx);
    abort();
}

int
mudband_log_printf(const char *id, int lvl, double t_elapsed, const char *msg)
{
    char nowstr[ODR_TIME_FORMAT_SIZE], line[1024];

    ODR_TimeFormat(nowstr, "%a, %d %b %Y %T GMT", ODR_real());
    snprintf(line, sizeof(line), "%s [%f] %-4s %s %s", nowstr, t_elapsed,
             id, vtc_lead(lvl), msg);
    switch (lvl) {
        case 0:
            __android_log_print(ANDROID_LOG_ERROR, "Mud.band", "%s", line);
            break;
        case 1:
            __android_log_print(ANDROID_LOG_WARN, "Mud.band", "%s", line);
            break;
        case 2:
            __android_log_print(ANDROID_LOG_INFO, "Mud.band", "%s", line);
            break;
        case 3:
        default:
            __android_log_print(ANDROID_LOG_DEBUG, "Mud.band", "%s", line);
            break;
    }
    return (1);
}

jstring
Java_band_mud_android_JniWrapper_getBandNameByUUID(JNIEnv *env, jobject thiz,
                                                   jstring mUUID)
{
    int r;
    char buf[64];
    const char *uuid;

    (void)thiz;

    uuid = (*env)->GetStringUTFChars(env, mUUID, NULL);
    AN(uuid);

    r = MBE_get_band_name_by_uuid(uuid, buf, sizeof(buf));
    if (r != 0) {
        (*env)->ReleaseStringUTFChars(env, mUUID, uuid);
        return (NULL);
    }
    (*env)->ReleaseStringUTFChars(env, mUUID, uuid);
    return (*env)->NewStringUTF(env, buf);
}

jstring
Java_band_mud_android_JniWrapper_getActiveBandName(JNIEnv *env, jobject thiz)
{
    int r;
    char buf[64];

    (void)thiz;

    r = MBE_get_band_name(buf, sizeof(buf));
    assert(r == 0);
    return (*env)->NewStringUTF(env, buf);
}

jstring
Java_band_mud_android_JniWrapper_getActiveDeviceName(JNIEnv *env, jobject thiz)
{
    struct cnf *cnf;
    jstring s;
    int r;
    const char *name;

    (void)thiz;

    r = CNF_get(&cnf);
    if (r == -1)
        return (NULL);
    name = CNF_get_interface_name(cnf->jroot);
    AN(name);
    s = (*env)->NewStringUTF(env, name);
    CNF_rel(&cnf);
    return (s);
}

jstring
Java_band_mud_android_JniWrapper_getActivePrivateIP(JNIEnv *env, jobject thiz)
{
    struct cnf *cnf;
    jstring s;
    int r;
    const char *private_ip;

    (void)thiz;

    r = CNF_get(&cnf);
    if (r == -1)
        return (NULL);
    private_ip = CNF_get_interface_private_ip(cnf->jroot);
    AN(private_ip);
    s = (*env)->NewStringUTF(env, private_ip);
    CNF_rel(&cnf);
    return (s);
}

jstring
Java_band_mud_android_JniWrapper_getBandJWT(JNIEnv *env, jobject thiz)
{
    const char *jwt;

    (void)thiz;

    jwt = MBE_get_jwt();
    AN(jwt);
    return (*env)->NewStringUTF(env, jwt);
}

jboolean
Java_band_mud_android_JniWrapper_isBandPublic(JNIEnv *env, jobject thiz)
{

    (void)thiz;

    if (!band_inited)
        return (0);
    return (MBE_is_public() != 0);
}

jstring
Java_band_mud_android_JniWrapper_getBandConfigEtag(JNIEnv *env, jobject thiz)
{
    struct cnf *cnf;
    jstring s;
    int r;
    const char *etag;

    (void)thiz;

    r = CNF_get(&cnf);
    if (r == -1)
        return (NULL);
    etag = CNF_get_etag(cnf->jroot);
    AN(etag);
    s = (*env)->NewStringUTF(env, etag);
    CNF_rel(&cnf);
    return (s);
}

jstring
Java_band_mud_android_JniWrapper_getBandConfigString(JNIEnv *env, jobject thiz)
{
    struct cnf *cnf;
    jstring s;
    int r;
    char *body;

    (void)thiz;

    r = CNF_get(&cnf);
    if (r == -1)
        return (NULL);
    AN(cnf->jroot);
    body = json_dumps(cnf->jroot, 0);
    s = (*env)->NewStringUTF(env, body);
    free(body);
    CNF_rel(&cnf);
    return (s);
}

jint
Java_band_mud_android_JniWrapper_parseConfigResponse(JNIEnv *env, jobject thiz,
                                                     jstring mEtag, jstring mBody)
{
    int r;
    const char *body, *etag = NULL;

    (void)thiz;

    body = (*env)->GetStringUTFChars(env, mBody, NULL);
    AN(body);
    if (mEtag != NULL) {
        etag = (*env)->GetStringUTFChars(env, mEtag, NULL);
        AN(etag);
    }
    r = CNF_parse_response(etag, body);
    (*env)->ReleaseStringUTFChars(env, mBody, body);
    return (r);
}

jint
Java_band_mud_android_JniWrapper_parseEnrollmentResponse(JNIEnv *env, jobject thiz,
                                                         jstring mPrivateKey, jstring mBody)
{
    int r;
    const char *body, *private_key;

    (void)thiz;

    body = (*env)->GetStringUTFChars(env, mBody, NULL);
    AN(body);
    private_key = (*env)->GetStringUTFChars(env, mPrivateKey, NULL);
    AN(private_key);
    r = MBE_parse_enroll_response(private_key, body);
    (*env)->ReleaseStringUTFChars(env, mBody, body);
    return (r);
}

void
Java_band_mud_android_JniWrapper_changeEnrollment(JNIEnv *env, jobject thiz,
                                                  jstring mUUID)
{
    int r;
    const char *uuid;

    uuid = (*env)->GetStringUTFChars(env, mUUID, NULL);
    AN(uuid);
    MPC_set_default_band_uuid(uuid);
    r = MBE_check_and_read();
    if (r != 0) {
        vtc_log(band_vl, 0, "BANDEC_00208: MBE_check_and_read() failed");
    }
    r = CNF_load();
    if (r != 0) {
        vtc_log(band_vl, 0, "BANDEC_00209: CNF_load() failed");
    }
    (*env)->ReleaseStringUTFChars(env, mUUID, uuid);
}

jint
Java_band_mud_android_JniWrapper_parseUnenrollmentResponse(JNIEnv *env, jobject thiz,
                                                           jstring mBody)
{
    int r;
    const char *body;

    (void)thiz;

    body = (*env)->GetStringUTFChars(env, mBody, NULL);
    AN(body);
    r = MBE_parse_unenroll_response(body);
    (*env)->ReleaseStringUTFChars(env, mBody, body);
    return (r);
}

JNIEXPORT void JNICALL
Java_band_mud_android_util_MudbandLog_00024Companion_log(JNIEnv *env,
                                                         jobject thiz,
                                                         jint level, jstring mMsg)
{
    const char *msg;

    (void)thiz;

    msg = (*env)->GetStringUTFChars(env, mMsg, NULL);
    AN(msg);
    vtc_log(band_ui_vl, level, msg);
    (*env)->ReleaseStringUTFChars(env, mMsg, msg);
}

jboolean
Java_band_mud_android_JniWrapper_isEnrolled(JNIEnv *env, jobject thiz)
{
    const char *default_band_uuid;

    (void)thiz;

    if (!band_inited)
        return (0);
    default_band_uuid = MPC_get_default_band_uuid();
    if (default_band_uuid == NULL)
        return (0);
    return (1);
}

#define MUDBAND_UUIDS_TRAVERSAL_DIR_ARG     32
struct mudband_uuids_traversal_dir_arg {
    int	        n_enroll;
    struct {
        char    str[64];
    } uuids[MUDBAND_UUIDS_TRAVERSAL_DIR_ARG];
};

static int
mudband_uuids_traversal_dir_callback(struct vtclog *vl, const char *name, void *orig_arg)
{
    struct mudband_uuids_traversal_dir_arg *arg =
            (struct mudband_uuids_traversal_dir_arg *)orig_arg;
    int nameLen;

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return (0);
    nameLen = strlen(name);
    if (nameLen < sizeof("band_0b0a3721-7dc0-4391-969d-b3b0d1e00925.json") - 1)
        return (0);
    if (strncmp(name, "band_", sizeof("band_") - 1) != 0)
        return (0);
    if (strcmp(name + nameLen - 5, ".json") != 0)
        return (0);
    if (arg->n_enroll >= MUDBAND_UUIDS_TRAVERSAL_DIR_ARG)
        return (0);
    ODR_snprintf(arg->uuids[arg->n_enroll].str, sizeof(arg->uuids[arg->n_enroll].str),
                 "%.*s", nameLen - 5 - (sizeof("band_") - 1),
                 name + sizeof("band_") - 1);
    arg->n_enroll++;
    return (0);
}

jobjectArray
Java_band_mud_android_JniWrapper_getBandUUIDs(JNIEnv *env, jobject thiz) {
    struct mudband_uuids_traversal_dir_arg dir_arg = {0,};
    jclass stringClass;
    jobjectArray stringArray;
    jstring stringElement;
    int i, r;
    bool success;

    (void) thiz;

    stringClass = (*env)->FindClass(env, "java/lang/String");
    if (stringClass == NULL)
        return (NULL);
    r = ODR_traversal_dir(band_vl, band_enroll_dir,
                          mudband_uuids_traversal_dir_callback, (void *) &dir_arg);
    if (r != 0) {
        vtc_log(band_vl, 0, "BANDEC_00210: ODR_traversal_dir() failed");
        return (NULL);
    }
    if (dir_arg.n_enroll == 0) {
        vtc_log(band_vl, 2, "No enrollments found.");
        return (NULL);
    }
    assert(dir_arg.n_enroll > 0);
    stringArray = (*env)->NewObjectArray(env, dir_arg.n_enroll, stringClass, NULL);
    if (stringArray == NULL)
        return (NULL);
    for (i = 0; i < dir_arg.n_enroll; i++) {
        stringElement = (*env)->NewStringUTF(env, dir_arg.uuids[i].str);
        AN(stringElement);
        (*env)->SetObjectArrayElement(env, stringArray, i, stringElement);
        (*env)->DeleteLocalRef(env, stringElement);
    }
    return (stringArray);
}

jobjectArray
Java_band_mud_android_JniWrapper_createWireguardKeys(JNIEnv *env, jobject thiz)
{
    jclass stringClass;
    jobjectArray stringArray;
    jstring stringElement;
    size_t wg_pubkeystrlen, wg_privkeystrlen;
    uint8_t wg_privkey[WIREGUARD_PRIVATE_KEY_LEN];
    uint8_t wg_pubkey[WIREGUARD_PUBLIC_KEY_LEN];
    char wg_pubkeystr[WIREGUARD_PUBLIC_KEY_LEN * 2 + 1 /* XXX */];
    char wg_privkeystr[WIREGUARD_PRIVATE_KEY_LEN * 2 + 1 /* XXX */];
    bool success;

    (void)thiz;

    /* generate wireguard key pair */
    wireguard_generate_private_key(wg_privkey);
    wireguard_generate_public_key(wg_pubkey, wg_privkey);
    wg_pubkeystrlen = sizeof(wg_pubkeystr);
    success = wireguard_base64_encode(wg_pubkey, sizeof(wg_pubkey),
                                      wg_pubkeystr, &wg_pubkeystrlen);
    if (!success) {
        vtc_log(band_vl, 0, "BANDEC_00211: wireguard_base64_encode() failed.");
        return (NULL);
    }
    wg_privkeystrlen = sizeof(wg_privkeystr);
    success = wireguard_base64_encode(wg_privkey, sizeof(wg_privkey),
                                      wg_privkeystr, &wg_privkeystrlen);
    if (!success) {
        vtc_log(band_vl, 0, "BANDEC_00212: wireguard_base64_encode() failed.");
        return (NULL);
    }

    stringClass = (*env)->FindClass(env, "java/lang/String");
    if (stringClass == NULL)
        return (NULL);
    stringArray = (*env)->NewObjectArray(env, 2, stringClass, NULL);
    if (stringArray == NULL)
        return (NULL);
    stringElement = (*env)->NewStringUTF(env, wg_pubkeystr);
    (*env)->SetObjectArrayElement(env, stringArray, 0, stringElement);
    (*env)->DeleteLocalRef(env, stringElement);
    stringElement = (*env)->NewStringUTF(env, wg_privkeystr);
    (*env)->SetObjectArrayElement(env, stringArray, 1, stringElement);
    (*env)->DeleteLocalRef(env, stringElement);
    return (stringArray);
}

jint
Java_band_mud_android_JniWrapper_getStunNatType(JNIEnv *env, jobject thiz)
{

    return ((int)STUNC_get_nattype());
}

jstring
Java_band_mud_android_JniWrapper_getStunMappedAddr(JNIEnv *env, jobject thiz)
{
    const char *addr;

    (void)thiz;

    addr = STUNC_get_mappped_addr();
    if (addr == NULL)
        return (NULL);
    return ((*env)->NewStringUTF(env, addr));
}

jint
Java_band_mud_android_JniWrapper_getListenPort(JNIEnv *env, jobject thiz)
{

    return (MCM_listen_port());
}

jobjectArray
Java_band_mud_android_JniWrapper_getIfAddrs(JNIEnv *env, jobject thiz)
{
    jclass stringClass;
    jobjectArray stringArray;
    json_t *addrs = NULL;
    int i;

    addrs = CNF_getifaddrs();
    if (addrs == NULL) {
        vtc_log(band_vl, 0, "BANDEC_00213: CNF_getifaddrs() failed.");
        return (NULL);
    }
    if (json_array_size(addrs) <= 0) {
        vtc_log(band_vl, 0, "BANDEC_00214: CNF_getifaddrs() returns zero size array.");
        return (NULL);
    }
    stringClass = (*env)->FindClass(env, "java/lang/String");
    if (stringClass == NULL)
        goto error;
    stringArray = (*env)->NewObjectArray(env, json_array_size(addrs), stringClass, NULL);
    if (stringArray == NULL)
        goto error;
    for (i = 0; i < json_array_size(addrs); i++) {
        json_t *addr;
        jstring stringElement;

        addr = json_array_get(addrs, i);
        AN(addr);
        assert(json_is_string(addr));
        stringElement = (*env)->NewStringUTF(env, json_string_value(addr));
        (*env)->SetObjectArrayElement(env, stringArray, i, stringElement);
        (*env)->DeleteLocalRef(env, stringElement);
    }
    json_decref(addrs);
    return (stringArray);
error:
    if (addrs != NULL) {
        json_decref(addrs);
    }
    return (NULL);
}

static int
subnet2cidr(const char *netmaskstr)
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

jstring
Java_band_mud_android_JniWrapper_getVpnServiceConfig(JNIEnv *env, jobject thiz)
{
    struct cnf *cnf;
    struct vsb *vsb;
    jstring s;
    int private_cidr, mtu, r;
    const char *private_ip, *private_mask;

    (void)thiz;

    r = CNF_get(&cnf);
    if (r == -1)
        return (NULL);
    vsb = vsb_newauto();
    AN(vsb);
    mtu = CNF_get_interface_mtu(cnf->jroot);
    vsb_printf(vsb, "m,%d", mtu);
    private_ip = CNF_get_interface_private_ip(cnf->jroot);
    AN(private_ip);
    private_mask = CNF_get_interface_private_mask(cnf->jroot);
    AN(private_mask);
    private_cidr = subnet2cidr(private_mask);
    vsb_printf(vsb, " a,%s,%d", private_ip, private_cidr);
    vsb_printf(vsb, " r,198.18.0.0,15");
    vsb_finish(vsb);
    s = (*env)->NewStringUTF(env, vsb_data(vsb));
    vsb_delete(vsb);
    CNF_rel(&cnf);
    return (s);
}

void
mudband_tunnel_iface_write(uint8_t *buf, size_t buflen)
{
    int l;

    l = ODR_write(band_tun_fd, buf, buflen);
    assert(l == buflen);
}

void
Java_band_mud_android_JniWrapper_tunnelInit(JNIEnv *env, jobject thiz, jint mTunFd)
{
    struct cnf *cnf;
    struct wireguard_iface_init_data init_data;
    int r;
    const char *private_ip;

    MQTT_subscribe();

    wireguard_init();
    band_tun_fd = mTunFd;

    r = CNF_get(&cnf);
    assert(r == 0);
    private_ip = CNF_get_interface_private_ip(cnf->jroot);
    AN(private_ip);

    init_data.private_ip = private_ip;
    init_data.private_key = MBE_get_private_key();
    init_data.listen_fd = MCM_listen_fd();
    band_device = wireguard_iface_init(band_vl, &init_data);
    AN(band_device);
    assert(band_device->udp_fd >= 0);
    CNF_rel(&cnf);

    band_need_iface_sync = 1;
}

static void
wireguard_iface_fini(struct wireguard_device *device)
{

    if (device == NULL)
        return;
    callout_stop(&device->cb, &device->co);
    COT_fini(&device->cb);
    if (device->peers != NULL)
        free(device->peers);
    free(device);
}

void
Java_band_mud_android_JniWrapper_tunnelFini(JNIEnv *env, jobject thiz)
{

    (void)env;
    (void)thiz;

    wireguard_iface_fini(band_device);
}


static int
tunnelLoopReturnCode(int errorCode)
{

    /* okay. signal to the VpnService thread if it needs. */
    if (band_need_fetch_config) {
        band_need_fetch_config = 0;
        return (1);
    }
    if (errorCode != 0)
        return (errorCode);
    return (0);
}

uint8_t *
mudband_tunnel_proxy_prepend_pkthdr(uint8_t *buf, size_t *buflen,
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
mudband_tunnel_proxy_handler(struct pbuf *p, struct wireguard_sockaddr *wsin)
{
    struct wireguard_proxy_pkthdr *pkthdr;
    vuuid_t band_uuid;
    int r;

    assert(p->len > sizeof(*pkthdr));
    pkthdr = (struct wireguard_proxy_pkthdr *)p->payload;
    if (pkthdr->f_version != 1) {
        wg_iface_stat.n_udp_proxy_rx_errs++;
        return (-1);
    }
    memcpy(&band_uuid, pkthdr->band_uuid,
           sizeof(band_uuid));
    r = VUUID_compare(&band_uuid, MBE_get_uuid());
    if (r != 0) {
        wg_iface_stat.n_udp_proxy_rx_errs++;
        return (-1);
    }
    wsin->proxy.src_addr = pkthdr->src_addr;
    wsin->proxy.dst_addr = pkthdr->dst_addr;
    p->payload += sizeof(*pkthdr);
    p->len -= sizeof(*pkthdr);
    wg_iface_stat.n_udp_proxy_rx_pkts++;
    wg_iface_stat.bytes_udp_proxy_rx += p->len;
    return (0);
}

jint
Java_band_mud_android_JniWrapper_tunnelLoop(JNIEnv *env, jobject thiz)
{
    struct timeval tv;
    struct wireguard_iphdr *iphdr;
    fd_set rset;
    ssize_t len;
    int errorCode = 0, maxfd, r;

    if (band_need_iface_sync) {
        band_need_iface_sync = 0;
        wireguard_iface_sync(band_device);
    }
    tv.tv_sec = 0;
    tv.tv_usec = 300000;
    FD_ZERO(&rset);
    FD_SET(band_tun_fd, &rset);
    maxfd = band_tun_fd;
    FD_SET(band_device->udp_fd, &rset);
    maxfd = MAX(maxfd, band_device->udp_fd);
    r = select(maxfd + 1, &rset, NULL, NULL, &tv);
    if (r == -1) {
        if (errno == EINTR) {
            errorCode = 100;
            goto done;
        }
        if (errno == EBADF) {
            errorCode = 101;
            goto done;
        }
        errorCode = 199;
        vtc_log(band_vl, 1, "BANDEC_00215: select() failed: %d %s", errno, strerror(errno));
        goto done;
    }
    if (r == 0)
        goto done;
    if (FD_ISSET(band_tun_fd, &rset)) {
        struct pbuf *p;

        p = pbuf_alloc(2048);
        AN(p);
        len = ODR_read(band_vl, band_tun_fd, p->payload, p->tot_len);
        if (len == -1) {
            pbuf_free(p);
            vtc_log(band_vl, 1, "BANDEC_00216: ODR_read() failed: %d %s", errno, strerror(errno));
            errorCode = 299;
            goto next;
        }
        assert(len >= 0);
        p->len = (size_t)len;
        iphdr = (struct wireguard_iphdr *)p->payload;
        if (WIREGUARD_IPHDR_HI_BYTE(iphdr->verlen) != 4) {
            wg_iface_stat.n_no_ipv4_hdr++;
            pbuf_free(p);
            goto next;
        }
        wg_iface_stat.n_tun_rx_pkts++;
        wg_iface_stat.bytes_tun_rx += p->len;
        wireguard_iface_output(band_device, p, iphdr->daddr);
        pbuf_free(p);
    }
next:
    if (FD_ISSET(band_device->udp_fd, &rset)) {
        struct pbuf *p;
        struct sockaddr_in sin;
        struct wireguard_sockaddr wsin;
        socklen_t sinlen;
        bool from_proxy = false;

        sinlen = sizeof(sin);
        p = pbuf_alloc(2048);
        AN(p);
        len = recvfrom(band_device->udp_fd, p->payload, p->tot_len, 0, (struct sockaddr *)&sin,
                       (socklen_t*)&sinlen);
        assert(len >= 0);
        p->len = (size_t)len;
        wg_iface_stat.n_udp_rx_pkts++;
        wg_iface_stat.bytes_udp_rx += p->len;
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
        wireguard_iface_network_rx(band_device, p, &wsin);
        pbuf_free(p);
    }
done:
    COT_ticks(&band_device->cb);
    COT_clock(&band_device->cb);

    return (tunnelLoopReturnCode(errorCode));
}

jint
Java_band_mud_android_JniWrapper_initJni(JNIEnv *env, jobject thiz, jstring mRootDir)
{
    int r;
    const char *root_dir;

    (void)thiz;

    AZ(pthread_mutex_lock(&band_mtx));
    if (band_inited) {
        AZ(pthread_mutex_unlock(&band_mtx));
        return (0);
    }
    band_inited = 1;
    AZ(pthread_mutex_unlock(&band_mtx));

    root_dir = (*env)->GetStringUTFChars(env, mRootDir, NULL);
    if (root_dir == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "Mud.band",
                            "BANDEC_00217: Failed to get the root_dir path.");
        return (-1);
    }
    snprintf(band_root_dir, sizeof(band_root_dir), "%s", root_dir);
    snprintf(band_enroll_dir, sizeof(band_enroll_dir), "%s/enroll", root_dir);
    (*env)->ReleaseStringUTFChars(env, mRootDir, root_dir);

    ODR_libinit();
    vtc_loginit();
    ODR_mkdir_recursive(band_enroll_dir);
    band_vl = vtc_logopen("band", mudband_log_printf);
    AN(band_vl);
    band_ui_vl = vtc_logopen("ui", mudband_log_printf);
    AN(band_ui_vl);

    PBUF_init();
    MBE_init();
    MPC_init();
    CNF_init();
    MCM_init();
    MQTT_init();
    STUNC_init();
    MWG_init();
    r = MBE_check_and_read();
    if (r != 0) {
        vtc_log(band_vl, 1, "BANDEC_00218: MBE_check_and_read() failed.");
    }

    MBT_init();
    vtc_log(band_vl, 2, "Initialized.");
    return (0);
}
