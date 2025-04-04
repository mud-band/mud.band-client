/*
  MIT License

  Copyright(c) 2018 Liam Bindle

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files(the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions :

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "odr.h"
#include "vassert.h"
#include "vtc_log.h"

#include "mudband_tunnel.h"
#include "mudband_tunnel_mqtt.h"

#define MQTT_MQ_CURRSZ(mq_ptr) \
    (mq_ptr->curr >= (uint8_t*) ((mq_ptr)->queue_tail - 1)) ? 0 : ((uint8_t*) ((mq_ptr)->queue_tail - 1)) - (mq_ptr)->curr
#define MQTT_PACKED_CSTRLEN(x)    \
    (2 + (unsigned int)strlen(x))
#define MQTT_MQ_GET(mq_ptr, index) \
    (((struct mqtt_queued_message*) ((mq_ptr)->mem_end)) - 1 - index)
#define MQTT_MQ_LENGTH(mq_ptr) \
    (((struct mqtt_queued_message*) ((mq_ptr)->mem_end)) - (mq_ptr)->queue_tail)
#define MQTT_BITFIELD_RULE_VIOLOATION(bitfield, rule_value, rule_mask) \
    ((bitfield ^ rule_value) & rule_mask)
#define MQTT_SUBSCRIBE_REQUEST_MAX_NUM_TOPICS 8
#define MQTT_GENERATE_STRING(STRING) #STRING,

enum mqtt_suback_return_code {
    MQTT_SUBACK_SUCCESS_MAX_QOS_0 = 0u,
    MQTT_SUBACK_SUCCESS_MAX_QOS_1 = 1u,
    MQTT_SUBACK_SUCCESS_MAX_QOS_2 = 2u,
    MQTT_SUBACK_FAILURE           = 128u
};

struct mqtt_fixed_header_rules_s {
    const uint8_t control_type_is_valid[16];
    const uint8_t required_flags[16];
    const uint8_t mask_required_flags[16];
};

struct mqtt_fixed_header_rules_s mqtt_fixed_header_rules = {
        {   /* boolean value, true if type is valid */
                0x00, /* MQTT_CONTROL_RESERVED */
                0x01, /* MQTT_CONTROL_CONNECT */
                0x01, /* MQTT_CONTROL_CONNACK */
                0x01, /* MQTT_CONTROL_PUBLISH */
                0x01, /* MQTT_CONTROL_PUBACK */
                0x01, /* MQTT_CONTROL_PUBREC */
                0x01, /* MQTT_CONTROL_PUBREL */
                0x01, /* MQTT_CONTROL_PUBCOMP */
                0x01, /* MQTT_CONTROL_SUBSCRIBE */
                0x01, /* MQTT_CONTROL_SUBACK */
                0x01, /* MQTT_CONTROL_UNSUBSCRIBE */
                0x01, /* MQTT_CONTROL_UNSUBACK */
                0x01, /* MQTT_CONTROL_PINGREQ */
                0x01, /* MQTT_CONTROL_PINGRESP */
                0x01, /* MQTT_CONTROL_DISCONNECT */
                0x00  /* MQTT_CONTROL_RESERVED */
        },
        {   /* flags that must be set for the associated control type */
                0x00, /* MQTT_CONTROL_RESERVED */
                0x00, /* MQTT_CONTROL_CONNECT */
                0x00, /* MQTT_CONTROL_CONNACK */
                0x00, /* MQTT_CONTROL_PUBLISH */
                0x00, /* MQTT_CONTROL_PUBACK */
                0x00, /* MQTT_CONTROL_PUBREC */
                0x02, /* MQTT_CONTROL_PUBREL */
                0x00, /* MQTT_CONTROL_PUBCOMP */
                0x02, /* MQTT_CONTROL_SUBSCRIBE */
                0x00, /* MQTT_CONTROL_SUBACK */
                0x02, /* MQTT_CONTROL_UNSUBSCRIBE */
                0x00, /* MQTT_CONTROL_UNSUBACK */
                0x00, /* MQTT_CONTROL_PINGREQ */
                0x00, /* MQTT_CONTROL_PINGRESP */
                0x00, /* MQTT_CONTROL_DISCONNECT */
                0x00  /* MQTT_CONTROL_RESERVED */
        },
        {   /* mask of flags that must be specific values for the associated control type*/
                0x00, /* MQTT_CONTROL_RESERVED */
                0x0F, /* MQTT_CONTROL_CONNECT */
                0x0F, /* MQTT_CONTROL_CONNACK */
                0x00, /* MQTT_CONTROL_PUBLISH */
                0x0F, /* MQTT_CONTROL_PUBACK */
                0x0F, /* MQTT_CONTROL_PUBREC */
                0x0F, /* MQTT_CONTROL_PUBREL */
                0x0F, /* MQTT_CONTROL_PUBCOMP */
                0x0F, /* MQTT_CONTROL_SUBSCRIBE */
                0x0F, /* MQTT_CONTROL_SUBACK */
                0x0F, /* MQTT_CONTROL_UNSUBSCRIBE */
                0x0F, /* MQTT_CONTROL_UNSUBACK */
                0x0F, /* MQTT_CONTROL_PINGREQ */
                0x0F, /* MQTT_CONTROL_PINGRESP */
                0x0F, /* MQTT_CONTROL_DISCONNECT */
                0x00  /* MQTT_CONTROL_RESERVED */
        }
};

static struct vtclog *mqtt_vl;
static struct mqtt_client mqtt_client;
static uint8_t mqtt_sendbuf[2048];
static uint8_t mqtt_recvbuf[1024];
static int mqtt_fd = -1;
static int mqtt_connected = 0;

static int mudband_tunnel_mqtt_connect(void);

static int
mqtt_open_sock(const char* addr, const char* port)
{
    struct addrinfo hints = { 0, };
    struct addrinfo *p, *servinfo;
    int fd = -1;
    int rv;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    rv = getaddrinfo(addr, port, &hints, &servinfo);
    if (rv != 0)
        return (-1);
    for(p = servinfo; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd == -1)
            continue;

        rv = connect(fd, p->ai_addr, p->ai_addrlen);
        if(rv == -1) {
            close(fd);
            fd = -1;
            continue;
        }
        break;
    }
    freeaddrinfo(servinfo);
    if (fd != -1)
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    return (fd);
}

static ssize_t
mqtt_pal_recvall(int fd, void* buf, size_t bufsz, int flags)
{
    const void *const start = buf;
    enum mqtt_error error = 0;
    ssize_t rv;
    do {
        rv = recv(fd, buf, bufsz, flags);
        if (rv == 0) {
            /*
             * recv returns 0 when the socket is (half) closed
             * by the peer.
             *
             * Raise an error to trigger a reconnect.
             */
            error = MQTT_ERROR_SOCKET_ERROR;
            break;
        }
        if (rv < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* should call recv later again */
                break;
            }
            /* an error occurred that wasn't "nothing to read". */
            error = MQTT_ERROR_SOCKET_ERROR;
            break;
        }
        buf = (char*)buf + rv;
        bufsz -= (unsigned long)rv;
    } while (bufsz > 0);
    if (buf == start) {
        return error;
    }
    return (char*)buf - (const char*)start;
}

static ssize_t
mqtt_pal_sendall(int fd, const void* buf, size_t len, int flags)
{
    enum mqtt_error error = 0;
    size_t sent = 0;
    while(sent < len) {
        ssize_t rv = send(fd, (const char*)buf + sent, len - sent, flags);
        if (rv < 0) {
            if (errno == EAGAIN) {
                /* should call send later again */
                break;
            }
            error = MQTT_ERROR_SOCKET_ERROR;
            break;
        }
        if (rv == 0) {
            /* is this possible? maybe OS bug. */
            error = MQTT_ERROR_SOCKET_ERROR;
            break;
        }
        sent += (size_t) rv;
    }
    if (sent == 0) {
        return error;
    }
    return (ssize_t)sent;
}

static ssize_t
mqtt_pack_uint16(uint8_t *buf, uint16_t integer)
{

    uint16_t integer_htons = htons(integer);
    memcpy(buf, &integer_htons, 2);
    return 2;
}

static ssize_t
mqtt_pack_str(uint8_t *buf, const char* str)
{
    uint16_t length = (uint16_t)strlen(str);
    int i = 0;
    /* pack string length */
    buf += mqtt_pack_uint16(buf, length);

    /* pack string */
    for(; i < length; ++i) {
        *(buf++) = (uint8_t)str[i];
    }
    
    /* return number of bytes consumed */
    return length + 2;
}

static void
mqtt_mq_init(struct mqtt_message_queue *mq, void *buf, size_t bufsz)
{

    if (buf != NULL) {
        mq->mem_start = buf;
        mq->mem_end = (uint8_t *)buf + bufsz;
        mq->curr = (uint8_t *)buf;
        mq->queue_tail = (struct mqtt_queued_message *)mq->mem_end;
        mq->curr_sz = (size_t) (MQTT_MQ_CURRSZ(mq));
    }
}

static enum mqtt_error
mqtt_init(struct mqtt_client *client, int sockfd,
    uint8_t *sendbuf, size_t sendbufsz,
    uint8_t *recvbuf, size_t recvbufsz,
    void (*publish_response_callback)(void** state,
    struct mqtt_response_publish *publish))
{

    if (client == NULL || sendbuf == NULL || recvbuf == NULL) {
        return MQTT_ERROR_NULLPTR;
    }

    client->socketfd = sockfd;

    mqtt_mq_init(&client->mq, sendbuf, sendbufsz);

    client->recv_buffer.mem_start = recvbuf;
    client->recv_buffer.mem_size = recvbufsz;
    client->recv_buffer.curr = client->recv_buffer.mem_start;
    client->recv_buffer.curr_sz = client->recv_buffer.mem_size;

    client->error = MQTT_ERROR_CONNECT_NOT_CALLED;
    client->response_timeout = 30;
    client->number_of_timeouts = 0;
    client->number_of_keep_alives = 0;
    client->typical_response_time = -1.0;
    client->publish_response_callback = publish_response_callback;
    client->pid_lfsr = 0;
    client->send_offset = 0;

    client->inspector_callback = NULL;
    client->reconnect_callback = NULL;
    client->reconnect_state = NULL;

    return MQTT_OK;
}

static ssize_t
mqtt_fixed_header_rule_violation(const struct mqtt_fixed_header *fixed_header)
{
    uint8_t control_type;
    uint8_t control_flags;
    uint8_t required_flags;
    uint8_t mask_required_flags;

    /* get value and rules */
    control_type = (uint8_t)fixed_header->control_type;
    control_flags = fixed_header->control_flags;
    required_flags = mqtt_fixed_header_rules.required_flags[control_type];
    mask_required_flags =
        mqtt_fixed_header_rules.mask_required_flags[control_type];

    /* check for valid type */
    if (!mqtt_fixed_header_rules.control_type_is_valid[control_type]) {
        return MQTT_ERROR_CONTROL_FORBIDDEN_TYPE;
    }
    
    /* check that flags are appropriate */
    if (MQTT_BITFIELD_RULE_VIOLOATION(control_flags, required_flags, mask_required_flags)) {
        return MQTT_ERROR_CONTROL_INVALID_FLAGS;
    }

    return 0;
}

static ssize_t
mqtt_pack_fixed_header(uint8_t *buf, size_t bufsz,
    const struct mqtt_fixed_header *fixed_header)
{
    const uint8_t *start = buf;
    ssize_t errcode;
    uint32_t remaining_length;
    
    /* check for null pointers or empty buffer */
    if (fixed_header == NULL || buf == NULL) {
        return MQTT_ERROR_NULLPTR;
    }

    /* check that the fixed header is valid */
    errcode = mqtt_fixed_header_rule_violation(fixed_header);
    if (errcode) {
        return errcode;
    }

    /* check that bufsz is not zero */
    if (bufsz == 0) return 0;

    /* pack control type and flags */
    *buf = (uint8_t)((((uint8_t) fixed_header->control_type) << 4) & 0xF0);
    *buf = (uint8_t)(*buf | (((uint8_t) fixed_header->control_flags) & 0x0F));

    remaining_length = fixed_header->remaining_length;

    /* MQTT spec (2.2.3) says maximum remaining length is 2^28-1 */
    if(remaining_length >= 256*1024*1024)
        return MQTT_ERROR_INVALID_REMAINING_LENGTH;

    do {
        /* consume byte and assert at least 1 byte left */
        --bufsz;
        ++buf;
        if (bufsz == 0) return 0;
        
        /* pack next byte */
        *buf  = remaining_length & 0x7F;
        if(remaining_length > 127) *buf |= 0x80;
        remaining_length = remaining_length >> 7;
    } while(*buf & 0x80);
    
    /* consume last byte */
    --bufsz;
    ++buf;

    /* check that there's still enough space in buffer for packet */
    if (bufsz < fixed_header->remaining_length) {
        return 0;
    }

    /* return how many bytes were consumed */
    return buf - start;
}

static ssize_t
mqtt_pack_connection_request(uint8_t* buf, size_t bufsz, const char* client_id,
    const char* will_topic, const void* will_message, size_t will_message_size,
    const char* user_name, const char* password, uint8_t connect_flags,
    uint16_t keep_alive)
{
    struct mqtt_fixed_header fixed_header;
    size_t remaining_length;
    const uint8_t *const start = buf;
    ssize_t rv;

    /* pack the fixed headr */
    fixed_header.control_type = MQTT_CONTROL_CONNECT;
    fixed_header.control_flags = 0x00;

    /* calculate remaining length and build connect_flags at the same time */
    connect_flags = (uint8_t) (connect_flags & ~MQTT_CONNECT_RESERVED);
    remaining_length = 10; /* size of variable header */

    if (client_id == NULL) {
        client_id = "";
    }
    /* For an empty client_id, a clean session is required */
    if (client_id[0] == '\0' &&
        !(connect_flags & MQTT_CONNECT_CLEAN_SESSION)) {
        return MQTT_ERROR_CLEAN_SESSION_IS_REQUIRED;
    }
    /* mqtt_string length is strlen + 2 */
    remaining_length += MQTT_PACKED_CSTRLEN(client_id);

    if (will_topic != NULL) {
        uint8_t temp;
        /* there is a will */
        connect_flags |= MQTT_CONNECT_WILL_FLAG;
        remaining_length += MQTT_PACKED_CSTRLEN(will_topic);
        
        if (will_message == NULL) {
            /* if there's a will there MUST be a will message */
            return MQTT_ERROR_CONNECT_NULL_WILL_MESSAGE;
        }
        /* size of will_message */
        remaining_length += 2 + will_message_size;

        /* assert that the will QOS is valid (i.e. not 3) */
        temp = connect_flags & 0x18; /* mask to QOS */
        if (temp == 0x18) {
            /* bitwise equality with QoS 3 (invalid)*/
            return MQTT_ERROR_CONNECT_FORBIDDEN_WILL_QOS;
        }
    } else {
        /* there is no will so set all will flags to zero */
        connect_flags &= (uint8_t)~MQTT_CONNECT_WILL_FLAG;
        connect_flags &= (uint8_t)~0x18;
        connect_flags &= (uint8_t)~MQTT_CONNECT_WILL_RETAIN;
    }

    if (user_name != NULL) {
        /* a user name is present */
        connect_flags |= MQTT_CONNECT_USER_NAME;
        remaining_length += MQTT_PACKED_CSTRLEN(user_name);
    } else {
        connect_flags &= (uint8_t)~MQTT_CONNECT_USER_NAME;
    }

    if (password != NULL) {
        /* a password is present */
        connect_flags |= MQTT_CONNECT_PASSWORD;
        remaining_length += MQTT_PACKED_CSTRLEN(password);
    } else {
        connect_flags &= (uint8_t)~MQTT_CONNECT_PASSWORD;
    }

    /* fixed header length is now calculated*/
    fixed_header.remaining_length = (uint32_t)remaining_length;

    /* pack fixed header and perform error checks */
    rv = mqtt_pack_fixed_header(buf, bufsz, &fixed_header);
    if (rv <= 0) {
        /* something went wrong */
        return rv;
    }
    buf += rv;
    bufsz -= (size_t)rv;

    /* check that the buffer has enough space to fit the remaining length */
    if (bufsz < fixed_header.remaining_length) {
        return 0;
    }

    /* pack the variable header */
    *buf++ = 0x00;
    *buf++ = 0x04;
    *buf++ = (uint8_t) 'M';
    *buf++ = (uint8_t) 'Q';
    *buf++ = (uint8_t) 'T';
    *buf++ = (uint8_t) 'T';
    *buf++ = MQTT_PROTOCOL_LEVEL;
    *buf++ = connect_flags;
    buf += mqtt_pack_uint16(buf, keep_alive);

    /* pack the payload */
    buf += mqtt_pack_str(buf, client_id);
    if (connect_flags & MQTT_CONNECT_WILL_FLAG) {
        buf += mqtt_pack_str(buf, will_topic);
        buf += mqtt_pack_uint16(buf, (uint16_t)will_message_size);
        memcpy(buf, will_message, will_message_size);
        buf += will_message_size;
    }
    if (connect_flags & MQTT_CONNECT_USER_NAME) {
        buf += mqtt_pack_str(buf, user_name);
    }
    if (connect_flags & MQTT_CONNECT_PASSWORD) {
        buf += mqtt_pack_str(buf, password);
    }

    /* return the number of bytes that were consumed */
    return buf - start;
}

static void
mqtt_mq_clean(struct mqtt_message_queue *mq)
{
    struct mqtt_queued_message *new_head;

    for (new_head = MQTT_MQ_GET(mq, 0); new_head >= mq->queue_tail;
         --new_head) {
        if (new_head->state != MQTT_QUEUED_COMPLETE)
            break;
    }
    
    /* check if everything can be removed */
    if (new_head < mq->queue_tail) {
        mq->curr = (uint8_t *)mq->mem_start;
        mq->queue_tail = (struct mqtt_queued_message *)mq->mem_end;
        mq->curr_sz = (size_t) (MQTT_MQ_CURRSZ(mq));
        return;
    } else if (new_head == MQTT_MQ_GET(mq, 0)) {
        /* do nothing */
        return;
    }

    /* move buffered data */
    {
        size_t n = (size_t) (mq->curr - new_head->start);
        size_t removing =
            (size_t) (new_head->start - (uint8_t*) mq->mem_start);
        memmove(mq->mem_start, new_head->start, n);
        mq->curr = (unsigned char*)mq->mem_start + n;
      

        /* move queue */
        {
            ssize_t new_tail_idx = new_head - mq->queue_tail;
            memmove(MQTT_MQ_GET(mq, new_tail_idx), mq->queue_tail,
                sizeof(struct mqtt_queued_message) * (size_t) ((new_tail_idx + 1)));
            mq->queue_tail = MQTT_MQ_GET(mq, new_tail_idx);
            {
                /* bump back start's */
                ssize_t i = 0;
                for(; i < new_tail_idx + 1; ++i) {
                    MQTT_MQ_GET(mq, i)->start -= removing;
                }
            }
        }
    }

    /* get curr_sz */
    mq->curr_sz = (size_t) (MQTT_MQ_CURRSZ(mq));
}

static struct mqtt_queued_message *
mqtt_mq_register(struct mqtt_message_queue *mq, size_t nbytes)
{
    /* make queued message header */
    --(mq->queue_tail);
    mq->queue_tail->start = mq->curr;
    mq->queue_tail->size = nbytes;
    mq->queue_tail->state = MQTT_QUEUED_UNSENT;

    /* move curr and recalculate curr_sz */
    mq->curr += nbytes;
    mq->curr_sz = (size_t) (MQTT_MQ_CURRSZ(mq));

    return mq->queue_tail;
}

#define MQTT_CLIENT_TRY_PACK(tmp, msg, client, pack_call) do {        \
    if (client->error < 0) {                                        \
        return client->error;                    \
    }                                                               \
    tmp = pack_call;                                                \
    if (tmp < 0) {                                                  \
        client->error = (enum mqtt_error)tmp;            \
        return (enum mqtt_error)tmp;                \
    } else if (tmp == 0) {                                          \
        mqtt_mq_clean(&client->mq);                \
        tmp = pack_call;                    \
        if (tmp < 0) {                        \
            client->error = (enum mqtt_error)tmp;        \
            return (enum mqtt_error)tmp;            \
        } else if(tmp == 0) {                    \
            client->error = MQTT_ERROR_SEND_BUFFER_IS_FULL;    \
            return (enum mqtt_error)client->error;        \
        }                            \
    }                                                               \
    msg = mqtt_mq_register(&client->mq, (size_t)tmp);        \
} while (0)

static enum mqtt_error
mqtt_connect(struct mqtt_client *client, const char* client_id,
    const char* will_topic, const void* will_message, size_t will_message_size,
    const char* user_name, const char* password, uint8_t connect_flags,
    uint16_t keep_alive)
{
    ssize_t rv;
    struct mqtt_queued_message *msg;

    /* update the client's state */
    client->keep_alive = keep_alive;
    if (client->error == MQTT_ERROR_CONNECT_NOT_CALLED) {
        client->error = MQTT_OK;
    }
    
    /* try to pack the message */
    MQTT_CLIENT_TRY_PACK(rv, msg, client,
        mqtt_pack_connection_request(client->mq.curr, client->mq.curr_sz,
        client_id, will_topic, will_message,
        will_message_size,user_name, password,
        connect_flags, keep_alive));
    /* save the control type of the message */
    msg->control_type = MQTT_CONTROL_CONNECT;

    return MQTT_OK;
}

static const char *MQTT_ERRORS_STR[] = {
    "MQTT_UNKNOWN_ERROR",
    MQTT_ALL_ERRORS(MQTT_GENERATE_STRING)
};

static const char *
mqtt_error_str(enum mqtt_error error)
{
    int offset = error - MQTT_ERROR_UNKNOWN;
    if (offset >= 0) {
        return MQTT_ERRORS_STR[offset];
    } else if (error == 0) {
        return "MQTT_ERROR: Buffer too small.";
    } else if (error > 0) {
        return "MQTT_OK";
    } else {
        return MQTT_ERRORS_STR[0];
    }
}

static uint16_t
mqtt_next_pid(struct mqtt_client *client)
{
    int pid_exists = 0;

    if (client->pid_lfsr == 0) {
        client->pid_lfsr = 163u;
    }
    /*
     * LFSR taps taken from:
     * https://en.wikipedia.org/wiki/Linear-feedback_shift_register
     */
    do {
        struct mqtt_queued_message *curr;
        unsigned lsb = client->pid_lfsr & 1;
        (client->pid_lfsr) >>= 1;
        if (lsb) {
            client->pid_lfsr ^= 0xB400u;
        }

        /* check that the PID is unique */
        pid_exists = 0;
        for (curr = MQTT_MQ_GET(&(client->mq), 0);
             curr >= client->mq.queue_tail; --curr) {
            if (curr->packet_id == client->pid_lfsr) {
                pid_exists = 1;
                break;
            }
        }

    } while(pid_exists);
    return client->pid_lfsr;
}

static ssize_t
mqtt_pack_subscribe_request(uint8_t *buf, size_t bufsz,
    unsigned int packet_id, ...)
{
    va_list args;
    const uint8_t *const start = buf;
    ssize_t rv;
    struct mqtt_fixed_header fixed_header;
    unsigned int num_subs = 0;
    unsigned int i;
    const char *topic[MQTT_SUBSCRIBE_REQUEST_MAX_NUM_TOPICS];
    uint8_t max_qos[MQTT_SUBSCRIBE_REQUEST_MAX_NUM_TOPICS];

    /* parse all subscriptions */
    va_start(args, packet_id);
    while(1) {
        topic[num_subs] = va_arg(args, const char*);
        if (topic[num_subs] == NULL) {
            /* end of list */
            break;
        }

        max_qos[num_subs] = (uint8_t) va_arg(args, unsigned int);

        ++num_subs;
        if (num_subs >= MQTT_SUBSCRIBE_REQUEST_MAX_NUM_TOPICS) {
            va_end(args);
            return MQTT_ERROR_SUBSCRIBE_TOO_MANY_TOPICS;
        }
    }
    va_end(args);

    /* build the fixed header */
    fixed_header.control_type = MQTT_CONTROL_SUBSCRIBE;
    fixed_header.control_flags = 2u;
    fixed_header.remaining_length = 2u; /* size of variable header */
    for(i = 0; i < num_subs; ++i) {
        /* payload is topic name + max qos (1 byte) */
        fixed_header.remaining_length += MQTT_PACKED_CSTRLEN(topic[i]) + 1;
    }

    /* pack the fixed header */
    rv = mqtt_pack_fixed_header(buf, bufsz, &fixed_header);
    if (rv <= 0) {
        return rv;
    }
    buf += rv;
    bufsz -= (unsigned long)rv;

    /* check that the buffer has enough space */
    if (bufsz < fixed_header.remaining_length) {
        return 0;
    }
    
    
    /* pack variable header */
    buf += mqtt_pack_uint16(buf, (uint16_t)packet_id);


    /* pack payload */
    for(i = 0; i < num_subs; ++i) {
        buf += mqtt_pack_str(buf, topic[i]);
        *buf++ = max_qos[i];
    }

    return buf - start;
}

static enum mqtt_error
mqtt_subscribe(struct mqtt_client *client,
    const char* topic_name,
    int max_qos_level)
{
    ssize_t rv;
    uint16_t packet_id;
    struct mqtt_queued_message *msg;

    packet_id = mqtt_next_pid(client);
    /* try to pack the message */
    MQTT_CLIENT_TRY_PACK(rv, msg, client,
        mqtt_pack_subscribe_request(client->mq.curr, client->mq.curr_sz,
        packet_id,
        topic_name,
        max_qos_level,
        (const char*)NULL));
    /* save the control type and packet id of the message */
    msg->control_type = MQTT_CONTROL_SUBSCRIBE;
    msg->packet_id = packet_id;
    return MQTT_OK;
}

static ssize_t
mqtt_unpack_connack_response(struct mqtt_response *mqtt_response,
    const uint8_t *buf)
{
    const uint8_t *const start = buf;
    struct mqtt_response_connack *response;

    /* check that remaining length is 2 */
    if (mqtt_response->fixed_header.remaining_length != 2) {
        return MQTT_ERROR_MALFORMED_RESPONSE;
    }
    
    response = &(mqtt_response->decoded.connack);
    /* unpack */
    if (*buf & 0xFE) {
        /* only bit 1 can be set */
        return MQTT_ERROR_CONNACK_FORBIDDEN_FLAGS;
    } else {
        response->session_present_flag = *buf++;
    }

    if (*buf > 5u) {
        /* only bit 1 can be set */
        return MQTT_ERROR_CONNACK_FORBIDDEN_CODE;
    } else {
        response->return_code = (enum mqtt_connack_return_code) *buf++;
    }
    return buf - start;
}

static ssize_t
mqtt_unpack_fixed_header(struct mqtt_response *response, const uint8_t *buf,
    size_t bufsz)
{
    struct mqtt_fixed_header *fixed_header;
    const uint8_t *start = buf;
    int lshift;
    ssize_t errcode;
    
    /* check for null pointers or empty buffer */
    if (response == NULL || buf == NULL) {
        return MQTT_ERROR_NULLPTR;
    }
    fixed_header = &(response->fixed_header);

    /* check that bufsz is not zero */
    if (bufsz == 0) return 0;

    /* parse control type and flags */
    fixed_header->control_type  = (enum mqtt_control_packet_type) (*buf >> 4);
    fixed_header->control_flags = (uint8_t) (*buf & 0x0F);

    /* parse remaining size */
    fixed_header->remaining_length = 0;

    lshift = 0;
    do {

        /* MQTT spec (2.2.3) says the maximum length is 28 bits */
        if(lshift == 28)
            return MQTT_ERROR_INVALID_REMAINING_LENGTH;

        /* consume byte and assert at least 1 byte left */
        --bufsz;
        ++buf;
        if (bufsz == 0) return 0;

        /* parse next byte*/
        fixed_header->remaining_length += (uint32_t) ((*buf & 0x7F) << lshift);
        lshift += 7;
    } while(*buf & 0x80); /* while continue bit is set */

    /* consume last byte */
    --bufsz;
    ++buf;

    /* check that the fixed header is valid */
    errcode = mqtt_fixed_header_rule_violation(fixed_header);
    if (errcode) {
        return errcode;
    }

    /* check that the buffer size if GT remaining length */
    if (bufsz < fixed_header->remaining_length) {
        return 0;
    }

    /* return how many bytes were consumed */
    return buf - start;
}

static uint16_t
mqtt_unpack_uint16(const uint8_t *buf)
{
    uint16_t integer_htons;

    memcpy(&integer_htons, buf, 2);
    return ntohs(integer_htons);
}

static ssize_t
mqtt_unpack_publish_response(struct mqtt_response *mqtt_response,
    const uint8_t *buf)
{
    const uint8_t *const start = buf;
    struct mqtt_fixed_header *fixed_header;
    struct mqtt_response_publish *response;
    
    fixed_header = &(mqtt_response->fixed_header);
    response = &(mqtt_response->decoded.publish);

    /* get flags */
    response->dup_flag = (fixed_header->control_flags & MQTT_PUBLISH_DUP) >> 3;
    response->qos_level = (fixed_header->control_flags & MQTT_PUBLISH_QOS_MASK) >> 1;
    response->retain_flag = fixed_header->control_flags & MQTT_PUBLISH_RETAIN;

    /* make sure that remaining length is valid */
    if (mqtt_response->fixed_header.remaining_length < 4) {
        return MQTT_ERROR_MALFORMED_RESPONSE;
    }

    /* parse variable header */
    response->topic_name_size = mqtt_unpack_uint16(buf);
    buf += 2;
    response->topic_name = buf;
    buf += response->topic_name_size;

    if (response->qos_level > 0) {
        response->packet_id = mqtt_unpack_uint16(buf);
        buf += 2;
    }

    /* get payload */
    response->application_message = buf;
    if (response->qos_level == 0) {
        response->application_message_size = fixed_header->remaining_length - response->topic_name_size - 2;
    } else {
        response->application_message_size = fixed_header->remaining_length - response->topic_name_size - 4;
    }
    buf += response->application_message_size;
    
    /* return number of bytes consumed */
    return buf - start;
}

static ssize_t
mqtt_unpack_pubxxx_response(struct mqtt_response *mqtt_response,
    const uint8_t *buf)
{
    const uint8_t *const start = buf;
    uint16_t packet_id;

    /* assert remaining length is correct */
    if (mqtt_response->fixed_header.remaining_length != 2) {
        return MQTT_ERROR_MALFORMED_RESPONSE;
    }

    /* parse packet_id */
    packet_id = mqtt_unpack_uint16(buf);
    buf += 2;

    if (mqtt_response->fixed_header.control_type == MQTT_CONTROL_PUBACK) {
        mqtt_response->decoded.puback.packet_id = packet_id;
    } else if (mqtt_response->fixed_header.control_type == MQTT_CONTROL_PUBREC) {
        mqtt_response->decoded.pubrec.packet_id = packet_id;
    } else if (mqtt_response->fixed_header.control_type == MQTT_CONTROL_PUBREL) {
        mqtt_response->decoded.pubrel.packet_id = packet_id;
    } else {
        mqtt_response->decoded.pubcomp.packet_id = packet_id;
    }

    return buf - start;
}

static ssize_t
mqtt_unpack_suback_response (struct mqtt_response *mqtt_response,
    const uint8_t *buf)
{
    const uint8_t *const start = buf;
    uint32_t remaining_length = mqtt_response->fixed_header.remaining_length;
    
    /*
     * assert remaining length is at least 3 (for packet id and
     * at least 1 topic)
     */
    if (remaining_length < 3) {
        return MQTT_ERROR_MALFORMED_RESPONSE;
    }

    /* unpack packet_id */
    mqtt_response->decoded.suback.packet_id = mqtt_unpack_uint16(buf);
    buf += 2;
    remaining_length -= 2;

    /* unpack return codes */
    mqtt_response->decoded.suback.num_return_codes =
        (size_t) remaining_length;
    mqtt_response->decoded.suback.return_codes = buf;
    buf += remaining_length;

    return buf - start;
}

static ssize_t
mqtt_unpack_unsuback_response(struct mqtt_response *mqtt_response,
    const uint8_t *buf)
{
    const uint8_t *const start = buf;

    if (mqtt_response->fixed_header.remaining_length != 2) {
        return MQTT_ERROR_MALFORMED_RESPONSE;
    }

    /* parse packet_id */
    mqtt_response->decoded.unsuback.packet_id = mqtt_unpack_uint16(buf);
    buf += 2;

    return buf - start;
}

static ssize_t
mqtt_unpack_response(struct mqtt_response* response, const uint8_t *buf,
    size_t bufsz)
{
    const uint8_t *const start = buf;
    ssize_t rv;

    rv = mqtt_unpack_fixed_header(response, buf, bufsz);
    if (rv <= 0)
        return rv;
    else
        buf += rv;
    switch(response->fixed_header.control_type) {
        case MQTT_CONTROL_CONNACK:
        rv = mqtt_unpack_connack_response(response, buf);
        break;
        case MQTT_CONTROL_PUBLISH:
        rv = mqtt_unpack_publish_response(response, buf);
        break;
        case MQTT_CONTROL_PUBACK:
        rv = mqtt_unpack_pubxxx_response(response, buf);
        break;
        case MQTT_CONTROL_PUBREC:
        rv = mqtt_unpack_pubxxx_response(response, buf);
        break;
        case MQTT_CONTROL_PUBREL:
        rv = mqtt_unpack_pubxxx_response(response, buf);
        break;
        case MQTT_CONTROL_PUBCOMP:
        rv = mqtt_unpack_pubxxx_response(response, buf);
        break;
        case MQTT_CONTROL_SUBACK:
        rv = mqtt_unpack_suback_response(response, buf);
        break;
        case MQTT_CONTROL_UNSUBACK:
        rv = mqtt_unpack_unsuback_response(response, buf);
        break;
        case MQTT_CONTROL_PINGRESP:
        return rv;
        default:
        return MQTT_ERROR_RESPONSE_INVALID_CONTROL_TYPE;
    }
    if (rv < 0)
        return rv;
    buf += rv;
    return buf - start;
}

static struct mqtt_queued_message *
mqtt_mq_find(struct mqtt_message_queue *mq,
    enum mqtt_control_packet_type control_type, uint16_t *packet_id)
{
    struct mqtt_queued_message *curr;

    for(curr = MQTT_MQ_GET(mq, 0); curr >= mq->queue_tail; --curr) {
        if (curr->control_type == control_type) {
            if ((packet_id == NULL &&
                 curr->state != MQTT_QUEUED_COMPLETE) ||
                (packet_id != NULL &&
                 *packet_id == curr->packet_id)) {
                return curr;
            }
        }
    }
    return NULL;
}

static ssize_t
mqtt_pack_pubxxx_request(uint8_t *buf, size_t bufsz,
    enum mqtt_control_packet_type control_type,
    uint16_t packet_id)
{
    const uint8_t *const start = buf;
    struct mqtt_fixed_header fixed_header;
    ssize_t rv;
    if (buf == NULL) {
        return MQTT_ERROR_NULLPTR;
    }

    /* pack fixed header */
    fixed_header.control_type = control_type;
    if (control_type == MQTT_CONTROL_PUBREL) {
        fixed_header.control_flags = 0x02;
    } else {
        fixed_header.control_flags = 0;
    }
    fixed_header.remaining_length = 2;
    rv = mqtt_pack_fixed_header(buf, bufsz, &fixed_header);
    if (rv <= 0) {
        return rv;
    }
    buf += rv;
    bufsz -= (size_t)rv;
    if (bufsz < fixed_header.remaining_length) {
        return 0;
    }
    buf += mqtt_pack_uint16(buf, packet_id);
    return buf - start;
}

static ssize_t
mqtt_puback(struct mqtt_client *client, uint16_t packet_id)
{
    ssize_t rv;
    struct mqtt_queued_message *msg;

    /* try to pack the message */
    MQTT_CLIENT_TRY_PACK(rv, msg, client,
        mqtt_pack_pubxxx_request(client->mq.curr, client->mq.curr_sz,
        MQTT_CONTROL_PUBACK,
        packet_id));
    /* save the control type and packet id of the message */
    msg->control_type = MQTT_CONTROL_PUBACK;
    msg->packet_id = packet_id;

    return MQTT_OK;
}

static ssize_t
mqtt_pubrec(struct mqtt_client *client, uint16_t packet_id)
{
    ssize_t rv;
    struct mqtt_queued_message *msg;

    /* try to pack the message */
    MQTT_CLIENT_TRY_PACK(rv, msg, client,
        mqtt_pack_pubxxx_request(client->mq.curr, client->mq.curr_sz,
        MQTT_CONTROL_PUBREC,
        packet_id));
    /* save the control type and packet id of the message */
    msg->control_type = MQTT_CONTROL_PUBREC;
    msg->packet_id = packet_id;

    return MQTT_OK;
}

static ssize_t
mqtt_pubrel(struct mqtt_client *client, uint16_t packet_id)
{
    ssize_t rv;
    struct mqtt_queued_message *msg;

    /* try to pack the message */
    MQTT_CLIENT_TRY_PACK(rv, msg, client,
        mqtt_pack_pubxxx_request(client->mq.curr, client->mq.curr_sz,
        MQTT_CONTROL_PUBREL,
        packet_id));
    /* save the control type and packet id of the message */
    msg->control_type = MQTT_CONTROL_PUBREL;
    msg->packet_id = packet_id;

    return MQTT_OK;
}

static ssize_t
mqtt_pubcomp(struct mqtt_client *client, uint16_t packet_id)
{
    ssize_t rv;
    struct mqtt_queued_message *msg;

    /* try to pack the message */
    MQTT_CLIENT_TRY_PACK(rv, msg, client,
        mqtt_pack_pubxxx_request(client->mq.curr, client->mq.curr_sz,
        MQTT_CONTROL_PUBCOMP,
        packet_id));
    /* save the control type and packet id of the message */
    msg->control_type = MQTT_CONTROL_PUBCOMP;
    msg->packet_id = packet_id;

    return MQTT_OK;
}

static ssize_t
mqtt_recv(struct mqtt_client *client)
{
    struct mqtt_response response;
    ssize_t mqtt_recv_ret = MQTT_OK;

    /* read until there is nothing left to read, or there was an error */
    while(mqtt_recv_ret == MQTT_OK) {
        /* read in as many bytes as possible */
        ssize_t rv, consumed;
        struct mqtt_queued_message *msg = NULL;

        rv = mqtt_pal_recvall(client->socketfd,
            client->recv_buffer.curr, client->recv_buffer.curr_sz, 0);
        if (rv < 0) {
            /* an error occurred */
            client->error = (enum mqtt_error)rv;
            return rv;
        } else {
            client->recv_buffer.curr += rv;
            client->recv_buffer.curr_sz -= (unsigned long)rv;
        }

        /* attempt to parse */
        consumed = mqtt_unpack_response(&response,
            client->recv_buffer.mem_start,
            (size_t)(client->recv_buffer.curr -
            client->recv_buffer.mem_start));
        if (consumed < 0) {
            client->error = (enum mqtt_error)consumed;
            return consumed;
        } else if (consumed == 0) {
            /*
             * if curr_sz is 0 then the buffer is too small to
             * ever fit the message
             */
            if (client->recv_buffer.curr_sz == 0) {
                client->error = MQTT_ERROR_RECV_BUFFER_TOO_SMALL;
                return MQTT_ERROR_RECV_BUFFER_TOO_SMALL;
            }

            /* just need to wait for the rest of the data */
            return MQTT_OK;
        }

        /* response was unpacked successfully */

        /*
          The switch statement below manages how the client responds
          to messages from the broker.

          Control Types (that we expect to receive from the broker):
          MQTT_CONTROL_CONNACK:
          -> release associated CONNECT
          -> handle response
          MQTT_CONTROL_PUBLISH:
          -> stage response, none if qos==0, PUBACK if qos==1,
             PUBREC if qos==2
          -> call publish callback
          MQTT_CONTROL_PUBACK:
          -> release associated PUBLISH
          MQTT_CONTROL_PUBREC:
          -> release PUBLISH
          -> stage PUBREL
          MQTT_CONTROL_PUBREL:
          -> release associated PUBREC
          -> stage PUBCOMP
          MQTT_CONTROL_PUBCOMP:
          -> release PUBREL
          MQTT_CONTROL_SUBACK:
          -> release SUBSCRIBE
          -> handle response
          MQTT_CONTROL_UNSUBACK:
          -> release UNSUBSCRIBE
          MQTT_CONTROL_PINGRESP:
          -> release PINGREQ
        */
        switch (response.fixed_header.control_type) {
        case MQTT_CONTROL_CONNACK:
            /* release associated CONNECT */
            msg = mqtt_mq_find(&client->mq, MQTT_CONTROL_CONNECT,
                NULL);
            if (msg == NULL) {
                client->error = MQTT_ERROR_ACK_OF_UNKNOWN;
                mqtt_recv_ret = MQTT_ERROR_ACK_OF_UNKNOWN;
                break;
            }
            msg->state = MQTT_QUEUED_COMPLETE;
            /* initialize typical response time */
            client->typical_response_time =
                (double)(time(NULL) - msg->time_sent);
            /* check that connection was successful */
            if (response.decoded.connack.return_code != MQTT_CONNACK_ACCEPTED) {
                if (response.decoded.connack.return_code == MQTT_CONNACK_REFUSED_IDENTIFIER_REJECTED) {
                    client->error = MQTT_ERROR_CONNECT_CLIENT_ID_REFUSED;
                    mqtt_recv_ret = MQTT_ERROR_CONNECT_CLIENT_ID_REFUSED;
                } else {
                    client->error = MQTT_ERROR_CONNECTION_REFUSED;
                    mqtt_recv_ret = MQTT_ERROR_CONNECTION_REFUSED;
                }
                break;
            }
            break;
        case MQTT_CONTROL_PUBLISH:
            /* stage response, none if qos==0, PUBACK if qos==1, PUBREC if qos==2 */
            if (response.decoded.publish.qos_level == 1) {
                rv = mqtt_puback(client, response.decoded.publish.packet_id);
                if (rv != MQTT_OK) {
                    client->error = (enum mqtt_error)rv;
                    mqtt_recv_ret = rv;
                    break;
                }
            } else if (response.decoded.publish.qos_level == 2) {
                /* check if this is a duplicate */
                if (mqtt_mq_find(&client->mq, MQTT_CONTROL_PUBREC, &response.decoded.publish.packet_id) != NULL) {
                    break;
                }

                rv = mqtt_pubrec(client, response.decoded.publish.packet_id);
                if (rv != MQTT_OK) {
                    client->error = (enum mqtt_error)rv;
                    mqtt_recv_ret = rv;
                    break;
                }
            }
            /* call publish callback */
            client->publish_response_callback(&client->publish_response_callback_state, &response.decoded.publish);
            break;
        case MQTT_CONTROL_PUBACK:
            /* release associated PUBLISH */
            msg = mqtt_mq_find(&client->mq, MQTT_CONTROL_PUBLISH, &response.decoded.puback.packet_id);
            if (msg == NULL) {
                client->error = MQTT_ERROR_ACK_OF_UNKNOWN;
                mqtt_recv_ret = MQTT_ERROR_ACK_OF_UNKNOWN;
                break;
            }
            msg->state = MQTT_QUEUED_COMPLETE;
            /* update response time */
            client->typical_response_time = 0.875 * (client->typical_response_time) + 0.125 * (double) (time(NULL) - msg->time_sent);
            break;
        case MQTT_CONTROL_PUBREC:
            /* check if this is a duplicate */
            if (mqtt_mq_find(&client->mq, MQTT_CONTROL_PUBREL, &response.decoded.pubrec.packet_id) != NULL) {
                break;
            }
            /* release associated PUBLISH */
            msg = mqtt_mq_find(&client->mq, MQTT_CONTROL_PUBLISH, &response.decoded.pubrec.packet_id);
            if (msg == NULL) {
                client->error = MQTT_ERROR_ACK_OF_UNKNOWN;
                mqtt_recv_ret = MQTT_ERROR_ACK_OF_UNKNOWN;
                break;
            }
            msg->state = MQTT_QUEUED_COMPLETE;
            /* update response time */
            client->typical_response_time = 0.875 * (client->typical_response_time) + 0.125 * (double) (time(NULL) - msg->time_sent);
            /* stage PUBREL */
            rv = mqtt_pubrel(client, response.decoded.pubrec.packet_id);
            if (rv != MQTT_OK) {
                client->error = (enum mqtt_error)rv;
                mqtt_recv_ret = rv;
                break;
            }
            break;
        case MQTT_CONTROL_PUBREL:
            /* release associated PUBREC */
            msg = mqtt_mq_find(&client->mq, MQTT_CONTROL_PUBREC, &response.decoded.pubrel.packet_id);
            if (msg == NULL) {
                client->error = MQTT_ERROR_ACK_OF_UNKNOWN;
                mqtt_recv_ret = MQTT_ERROR_ACK_OF_UNKNOWN;
                break;
            }
            msg->state = MQTT_QUEUED_COMPLETE;
            /* update response time */
            client->typical_response_time = 0.875 * (client->typical_response_time) + 0.125 * (double) (time(NULL) - msg->time_sent);
            /* stage PUBCOMP */
            rv = mqtt_pubcomp(client, response.decoded.pubrec.packet_id);
            if (rv != MQTT_OK) {
                client->error = (enum mqtt_error)rv;
                mqtt_recv_ret = rv;
                break;
            }
            break;
        case MQTT_CONTROL_PUBCOMP:
            /* release associated PUBREL */
            msg = mqtt_mq_find(&client->mq, MQTT_CONTROL_PUBREL, &response.decoded.pubcomp.packet_id);
            if (msg == NULL) {
                client->error = MQTT_ERROR_ACK_OF_UNKNOWN;
                mqtt_recv_ret = MQTT_ERROR_ACK_OF_UNKNOWN;
                break;
            }
            msg->state = MQTT_QUEUED_COMPLETE;
            /* update response time */
            client->typical_response_time = 0.875 * (client->typical_response_time) + 0.125 * (double) (time(NULL) - msg->time_sent);
            break;
        case MQTT_CONTROL_SUBACK:
            /* release associated SUBSCRIBE */
            msg = mqtt_mq_find(&client->mq, MQTT_CONTROL_SUBSCRIBE, &response.decoded.suback.packet_id);
            if (msg == NULL) {
                client->error = MQTT_ERROR_ACK_OF_UNKNOWN;
                mqtt_recv_ret = MQTT_ERROR_ACK_OF_UNKNOWN;
                break;
            }
            msg->state = MQTT_QUEUED_COMPLETE;
            /* update response time */
            client->typical_response_time = 0.875 * (client->typical_response_time) + 0.125 * (double) (time(NULL) - msg->time_sent);
            /* check that subscription was successful (not currently only one subscribe at a time) */
            if (response.decoded.suback.return_codes[0] == MQTT_SUBACK_FAILURE) {
                client->error = MQTT_ERROR_SUBSCRIBE_FAILED;
                mqtt_recv_ret = MQTT_ERROR_SUBSCRIBE_FAILED;
                break;
            }
            break;
        case MQTT_CONTROL_UNSUBACK:
            /* release associated UNSUBSCRIBE */
            msg = mqtt_mq_find(&client->mq, MQTT_CONTROL_UNSUBSCRIBE, &response.decoded.unsuback.packet_id);
            if (msg == NULL) {
                client->error = MQTT_ERROR_ACK_OF_UNKNOWN;
                mqtt_recv_ret = MQTT_ERROR_ACK_OF_UNKNOWN;
                break;
            }
            msg->state = MQTT_QUEUED_COMPLETE;
            /* update response time */
            client->typical_response_time = 0.875 * (client->typical_response_time) + 0.125 * (double) (time(NULL) - msg->time_sent);
            break;
        case MQTT_CONTROL_PINGRESP:
            /* release associated PINGREQ */
            msg = mqtt_mq_find(&client->mq, MQTT_CONTROL_PINGREQ, NULL);
            if (msg == NULL) {
                client->error = MQTT_ERROR_ACK_OF_UNKNOWN;
                mqtt_recv_ret = MQTT_ERROR_ACK_OF_UNKNOWN;
                break;
            }
            msg->state = MQTT_QUEUED_COMPLETE;
            /* update response time */
            client->typical_response_time = 0.875 * (client->typical_response_time) + 0.125 * (double) (time(NULL) - msg->time_sent);
            break;
        default:
            client->error = MQTT_ERROR_MALFORMED_RESPONSE;
            mqtt_recv_ret = MQTT_ERROR_MALFORMED_RESPONSE;
            break;
        }
        {
            /* we've handled the response, now clean the buffer */
            void* dest = (unsigned char*)client->recv_buffer.mem_start;
            void* src  = (unsigned char*)client->recv_buffer.mem_start + consumed;
            size_t n = (size_t) (client->recv_buffer.curr - client->recv_buffer.mem_start - consumed);
            memmove(dest, src, n);
            client->recv_buffer.curr -= consumed;
            client->recv_buffer.curr_sz += (unsigned long)consumed;
        }
    }

    /*
     * In case there was some error handling the (well formed) message,
     * we end up here
     */
    return mqtt_recv_ret;
}

static ssize_t
mqtt_pack_ping_request(uint8_t *buf, size_t bufsz)
{
    struct mqtt_fixed_header fixed_header;

    fixed_header.control_type = MQTT_CONTROL_PINGREQ;
    fixed_header.control_flags = 0;
    fixed_header.remaining_length = 0;
    return mqtt_pack_fixed_header(buf, bufsz, &fixed_header);
}

static enum mqtt_error
mqtt_ping(struct mqtt_client *client)
{
    ssize_t rv;
    struct mqtt_queued_message *msg;

    /* try to pack the message */
    MQTT_CLIENT_TRY_PACK(rv, msg, client,
        mqtt_pack_ping_request(client->mq.curr, client->mq.curr_sz));
    /* save the control type and packet id of the message */
    msg->control_type = MQTT_CONTROL_PINGREQ;
    
    return MQTT_OK;
}

static ssize_t
mqtt_send(struct mqtt_client *client)
{
    uint8_t inspected;
    ssize_t len;
    int inflight_qos2 = 0;
    int i = 0;
    
    if (client->error < 0 &&
        client->error != MQTT_ERROR_SEND_BUFFER_IS_FULL) {
        return client->error;
    }

    /* loop through all messages in the queue */
    len = MQTT_MQ_LENGTH(&client->mq);
    for(; i < len; ++i) {
        struct mqtt_queued_message *msg = MQTT_MQ_GET(&client->mq, i);
        int resend = 0;
        if (msg->state == MQTT_QUEUED_UNSENT) {
            /* message has not been sent to lets send it */
            resend = 1;
        } else if (msg->state == MQTT_QUEUED_AWAITING_ACK) {
            /* check for timeout */
            if (time(NULL) > msg->time_sent + client->response_timeout) {
                resend = 1;
                client->number_of_timeouts += 1;
                client->send_offset = 0;
            }
        }

        /* only send QoS 2 message if there are no inflight QoS 2 PUBLISH messages */
        if (msg->control_type == MQTT_CONTROL_PUBLISH
            && (msg->state == MQTT_QUEUED_UNSENT || msg->state == MQTT_QUEUED_AWAITING_ACK))
            {
                inspected = 0x03 & ((msg->start[0]) >> 1); /* qos */
                if (inspected == 2) {
                    if (inflight_qos2) {
                        resend = 0;
                    }
                    inflight_qos2 = 1;
                }
            }

        /* goto next message if we don't need to send */
        if (!resend) {
            continue;
        }

        /* we're sending the message */
        {
            ssize_t tmp = mqtt_pal_sendall(client->socketfd, msg->start + client->send_offset, msg->size - client->send_offset, 0);
            if (tmp < 0) {
                client->error = (enum mqtt_error)tmp;
                return tmp;
            } else {
                client->send_offset += (unsigned long)tmp;
                if(client->send_offset < msg->size) {
                    /* partial sent. Await additional calls */
                    break;
                } else {
                    /* whole message has been sent */
                    client->send_offset = 0;
                }

            }

        }

        /* update timeout watcher */
        client->time_of_last_send = time(NULL);
        msg->time_sent = client->time_of_last_send;

        /*
           Determine the state to put the message in.
           Control Types:
           MQTT_CONTROL_CONNECT     -> awaiting
           MQTT_CONTROL_CONNACK     -> n/a
           MQTT_CONTROL_PUBLISH     -> qos == 0 ? complete : awaiting
           MQTT_CONTROL_PUBACK      -> complete
           MQTT_CONTROL_PUBREC      -> awaiting
           MQTT_CONTROL_PUBREL      -> awaiting
           MQTT_CONTROL_PUBCOMP     -> complete
           MQTT_CONTROL_SUBSCRIBE   -> awaiting
           MQTT_CONTROL_SUBACK      -> n/a
           MQTT_CONTROL_UNSUBSCRIBE -> awaiting
           MQTT_CONTROL_UNSUBACK    -> n/a
           MQTT_CONTROL_PINGREQ     -> awaiting
           MQTT_CONTROL_PINGRESP    -> n/a
           MQTT_CONTROL_DISCONNECT  -> complete
        */
        switch (msg->control_type) {
        case MQTT_CONTROL_PUBACK:
        case MQTT_CONTROL_PUBCOMP:
        case MQTT_CONTROL_DISCONNECT:
            msg->state = MQTT_QUEUED_COMPLETE;
            break;
        case MQTT_CONTROL_PUBLISH:
            /* qos */
            inspected =
                (MQTT_PUBLISH_QOS_MASK & (msg->start[0])) >> 1;
            if (inspected == 0) {
                msg->state = MQTT_QUEUED_COMPLETE;
            } else if (inspected == 1) {
                msg->state = MQTT_QUEUED_AWAITING_ACK;
                /*
                 * set DUP flag for subsequent sends
                 * [Spec MQTT-3.3.1-1]
                 */
                msg->start[0] |= MQTT_PUBLISH_DUP;
            } else {
                msg->state = MQTT_QUEUED_AWAITING_ACK;
            }
            break;
        case MQTT_CONTROL_CONNECT:
        case MQTT_CONTROL_PUBREC:
        case MQTT_CONTROL_PUBREL:
        case MQTT_CONTROL_SUBSCRIBE:
        case MQTT_CONTROL_UNSUBSCRIBE:
        case MQTT_CONTROL_PINGREQ:
            msg->state = MQTT_QUEUED_AWAITING_ACK;
            break;
        default:
            client->error = MQTT_ERROR_MALFORMED_REQUEST;
            return MQTT_ERROR_MALFORMED_REQUEST;
        }
    }

    /* check for keep-alive */
    {
        time_t keep_alive_timeout = client->time_of_last_send +
            (time_t)((float)(client->keep_alive));
        if (time(NULL) > keep_alive_timeout) {
            ssize_t rv = mqtt_ping(client);
            if (rv != MQTT_OK) {
                client->error = (enum mqtt_error)rv;
                return rv;
            }
        }
    }
    return MQTT_OK;
}

static enum mqtt_error
mqtt_sync(struct mqtt_client *client)
{
    enum mqtt_error err;
    int reconnecting = 0;

    if (client->error != MQTT_ERROR_RECONNECTING &&
        client->error != MQTT_OK &&
        client->reconnect_callback != NULL) {
        client->reconnect_callback(client, &client->reconnect_state);
    } else {
        /*
         * mqtt_reconnect will have queued the disconnect packet -
         * that needs to be sent and then call reconnect
         */
        if (client->error == MQTT_ERROR_RECONNECTING) {
            reconnecting = 1;
            client->error = MQTT_OK;
        }
    }

    /* Call inspector callback if necessary */
    if (client->inspector_callback != NULL) {
        err = client->inspector_callback(client);
        if (err != MQTT_OK) return err;
    }

    /* Call receive */
    err = (enum mqtt_error)mqtt_recv(client);
    if (err != MQTT_OK)
        return err;

    /* Call send */
    err = (enum mqtt_error)mqtt_send(client);
    /*
     * mqtt_reconnect will essentially be a disconnect if there is
     * no callback
     */
    if (reconnecting && client->reconnect_callback != NULL) {
        client->reconnect_callback(client, &client->reconnect_state);
    }

    return err;
}

static void
mqtt_publish_callback(void** unused, struct mqtt_response_publish *published)
{
    json_t *jroot, *jevent;
    json_error_t jerror;

    (void)unused;

    vtc_log(mqtt_vl, 2, "Received publish('%.*s'): %.*s",
        (int)published->topic_name_size,
        (const char *)published->topic_name,
        (int)published->application_message_size,
        (const char *)published->application_message);

    jroot = json_loadb(published->application_message,
        published->application_message_size, 0, &jerror);
    if (jroot == NULL) {
        vtc_log(mqtt_vl, 0, "BANDEC_00543: Failed to parse JSON: %s",
		jerror.text);
        return;
    }
    assert(json_is_object(jroot));
    jevent = json_object_get(jroot, "event");
    AN(jevent);
    assert(json_is_string(jevent));
    if (strcmp(json_string_value(jevent), "conf") == 0) {
        mudband_tunnek_tasks_conf_fetcher_trigger();
    } else {
        vtc_log(mqtt_vl, 0, "BANDEC_00544: Unexpected event: %s",
		json_string_value(jevent));
    }
    json_decref(jroot);
}

static void
mudband_tunnel_mqtt_disconnect(void)
{

	close(mqtt_fd);
	memset(&mqtt_client, 0, sizeof(mqtt_client));

	vtc_log(mqtt_vl, 2, "Disconnected from MQTT broker.");

	mqtt_connected = 0;
}

static void
mudband_tunnel_mqtt_reconnect(void)
{
	int r;

	vtc_log(mqtt_vl, 2, "Trying to reconnect to MQTT broker.");

	mudband_tunnel_mqtt_disconnect();
	r = mudband_tunnel_mqtt_connect();
	if (r != 0) {
		vtc_log(mqtt_vl, 0,
		    "BANDEC_00840: Failed to reconnect to MQTT broker.");
		return;
	}
	mudband_tunnel_mqtt_subscribe();
}

void
mudband_tunnel_mqtt_sync(void)
{
    enum mqtt_error error;

    if (mqtt_connected == 0) {
        mudband_tunnel_mqtt_reconnect();
    }
    error = mqtt_sync(&mqtt_client);
    switch (error) {
    case MQTT_OK:
        break;
    default:
        vtc_log(mqtt_vl, 0, "BANDEC_00432: mqtt_sync() failed: %s",
		mqtt_error_str(error));
	mudband_tunnel_mqtt_reconnect();
        break;
    }
}

void
mudband_tunnel_mqtt_subscribe(void)
{
    struct mudband_tunnel_bandconf *cnf;
    enum mqtt_error error;
    int r;
    const char *band_uuid;
    char topic[128];

    band_uuid = mudband_tunnel_progconf_get_default_band_uuidstr();
    if (band_uuid == NULL) {
        vtc_log(mqtt_vl, 0, "BANDEC_00743: Failed to get default band UUID.");
	return;
    }
    snprintf(topic, sizeof(topic), "/band/%s", band_uuid);

    error = mqtt_subscribe(&mqtt_client, topic, 0);
    assert(error == MQTT_OK);

    vtc_log(mqtt_vl, 2, "Subscribed to %s topic.", topic);

    r = mudband_tunnel_confmgr_get(&cnf);
    if (r == 0) {
        const char *device_uuid;

        device_uuid = mudband_tunnel_confmgr_get_interface_device_uuid(cnf->jroot);
        AN(device_uuid);
        snprintf(topic, sizeof(topic), "/band/%s/device/%s",
                 band_uuid, device_uuid);

        error = mqtt_subscribe(&mqtt_client, topic, 0);
        assert(error == MQTT_OK);

        vtc_log(mqtt_vl, 2, "Subscribed to %s topic.", topic);

        mudband_tunnel_confmgr_rel(&cnf);
    }
}

static int
mudband_tunnel_mqtt_connect(void)
{
    enum mqtt_error error;
    const char *client_id = NULL;
    uint8_t connect_flags = MQTT_CONNECT_CLEAN_SESSION;

    mqtt_fd = mqtt_open_sock("mqtt.mud.band", "1883");
    if (mqtt_fd == -1) {
        vtc_log(mqtt_vl, 0,
		"BANDEC_00682: Failed to connect to MQTT broker.");
	return (-1);
    }
    error = mqtt_init(&mqtt_client, mqtt_fd,
		      mqtt_sendbuf, sizeof(mqtt_sendbuf),
		      mqtt_recvbuf, sizeof(mqtt_recvbuf),
		      mqtt_publish_callback);
    assert(error == MQTT_OK);	
    error = mqtt_connect(&mqtt_client, client_id, NULL, NULL, 0,
			 "mudband", "20241127", connect_flags, 400);
    assert(error == MQTT_OK);
    assert(mqtt_client.error == MQTT_OK);

    vtc_log(mqtt_vl, 2, "Connected to MQTT broker.");

    mqtt_connected = 1;
    return (0);
}

int
mudband_tunnel_mqtt_init(void)
{
    int r;

    mqtt_vl = vtc_logopen("mqtt", mudband_tunnel_log_callback);
    AN(mqtt_vl);
    r = mudband_tunnel_mqtt_connect();
    if (r != 0) {
	vtc_log(mqtt_vl, 0,
		"BANDEC_00841: Failed to connect to MQTT broker.");
	return (-1);
    }
    return (0);
}
