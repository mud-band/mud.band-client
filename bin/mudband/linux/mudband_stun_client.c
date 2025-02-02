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

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mudband.h"
#include "mudband_stun_client.h"

#include "odr.h"
#include "vtc_log.h"

static struct vtclog *stunc_vl;

#define STUN_MAX_STRING		256
#define STUN_MAX_UNKNOWN_ATTRIBUTES 8
#define	STUN_MAX_MESSAGE_SIZE	2048

#define STUN_A_IPV4FAMILY	0x01
#define STUN_A_IPV6FAMILY	0x02
#define STUN_R_MAPPEDADDRESS    0x0001
#define STUN_R_RESPONSEADDRESS  0x0002
#define STUN_R_CHANGEREQUEST    0x0003
#define STUN_R_SOURCEADDRESS    0x0004
#define STUN_R_CHANGEDADDRESS   0x0005
#define STUN_R_USERNAME         0x0006
#define STUN_R_PASSWORD         0x0007
#define STUN_R_MESSAGEINTEGRITY 0x0008
#define STUN_R_ERRORCODE        0x0009
#define STUN_R_UNKNOWNATTRIBUTE 0x000A
#define STUN_R_REFLECTEDFROM    0x000B
#define STUN_R_XORMAPPEDADDRESS 0x8020
#define STUN_R_XORONLY          0x0021
#define STUN_R_SERVERNAME       0x8022
#define STUN_T_BINDREQUESTMSG	0x0001
#define STUN_F_CHANGEIP		0x04
#define STUN_F_CHANGEPORT	0x02

enum stun_sm_return {
	STUN_SM_RETURN_ABORT,
	STUN_SM_RETURN_CONTINUE,
	STUN_SM_RETURN_WAIT,
};

enum stun_step {
	STUN_STEP_FIRST = 0,
	STUN_STEP_TEST_I_PREPARE,	/* STUN connect test */
	STUN_STEP_TEST_I_SEND,
	STUN_STEP_TEST_I_RECV,
	STUN_STEP_TEST_I2_PREPARE,	/* Same IP mapping test */
	STUN_STEP_TEST_I2_SEND,
	STUN_STEP_TEST_I2_RECV,
	STUN_STEP_TEST_I3_PREPARE,	/* Hairpin NAT test */
	STUN_STEP_TEST_I3_SEND,
	STUN_STEP_TEST_I3_RECV,
	STUN_STEP_TEST_II_PREPARE,	/* IP change test */
	STUN_STEP_TEST_II_SEND,
	STUN_STEP_TEST_II_RECV,
	STUN_STEP_TEST_III_PREPARE,	/* Port change test */
	STUN_STEP_TEST_III_SEND,
	STUN_STEP_TEST_III_RECV,
	STUN_STEP_TIMEOUT,
	STUN_STEP_ERROR,
	STUN_STEP_DONE
};

typedef struct {
	uint8_t		octet[16];
} uint128_t;

struct stun_msghdr {
	uint16_t	msg_type;
	uint16_t	msg_length;
	uint128_t	id;
};

struct stun_addr4 {
	uint16_t	port;
	uint32_t	addr;
};

struct stun_attr_hdr {
	uint16_t	type;
	uint16_t	length;
};

struct stun_attr_addr4 {
	uint8_t		pad;
	uint8_t		family;
	struct stun_addr4 ipv4;
};

struct stun_attr_changerequest {
	uint32_t	value;
};

struct stun_attr_string {
	char		value[STUN_MAX_STRING];      
	uint16_t	size_value;
};

struct stun_attr_unknown {
	uint16_t	attr_type[STUN_MAX_UNKNOWN_ATTRIBUTES];
	uint16_t	num_attributes;
};

struct stun_attr_error {
	uint16_t	pad;
	uint8_t		error_class;
	uint8_t		number;
	char		reason[STUN_MAX_STRING];
	uint16_t	size_reason;
};

struct stun_attr_integrity {
      char		hash[20];
};

struct stun_msg {
	struct stun_msghdr msg_hdr;

	unsigned	has_mapped_address : 1,
			has_response_address : 1,
			has_change_request : 1,
			has_source_address : 1,
			has_changed_address : 1,
			has_username : 1,
			has_password : 1,
			has_message_integrity : 1,
			has_error_code : 1,
			has_unknown_attributes : 1,
			has_reflected_from : 1,
			has_xor_mapped_address : 1,
			xor_only : 1,
			has_server_name : 1,
			unused: 18;
	struct stun_attr_addr4 mapped_address;
	struct stun_attr_addr4 response_address;
	struct stun_attr_changerequest change_request;
	struct stun_attr_addr4 source_address;
	struct stun_attr_addr4 changed_address;
	struct stun_attr_string username;
	struct stun_attr_string password;
	struct stun_attr_integrity message_integrity;
	struct stun_attr_error error_code;
	struct stun_attr_unknown unknown_attributes;
	struct stun_attr_addr4 reflectedFrom;
	struct stun_attr_addr4 xor_mapped_address;
	struct stun_attr_string serverName;
};

struct stun_client {
	enum stun_step		step;
	enum stun_step		step_previous;
	struct stun_addr4	src;
	struct stun_addr4	dst;
	int			fd;

	struct stun_attr_string username;
	struct stun_attr_string password;

	struct {
		struct stun_addr4 mapped_addr;
	} test_i;

	struct {
		struct stun_addr4 dst;
	} test_i2;

	struct {
		int		test_i_success;
		int		test_i2_success;
		int		test_i3_success;
		int		test_ii_success;
		int		test_ii_fail_no_ip_change;
		int		test_iii_success;
		int		test_iii_fail_no_port_change;
		int		preserve_port;
		int		hairpin;
		int		mapped_same_ip;
		int		is_nat;
	} result;
};

static struct stun_client_result stunc_result;
static int stunc_result_inited;

struct stun_addr4
	stun_client_get_mapped_addr(struct stun_client *sc);
enum stun_nattype
	stun_client_get_nattype(struct stun_client *sc);
void	stun_client_perform(struct stun_client *sc);

static char *
stun_encode16(char *buf, uint16_t data)
{
	uint16_t ndata;

	ndata = htons(data);
	memcpy(buf, (void *)&ndata, sizeof(uint16_t));
	return (buf + sizeof(uint16_t));
}

static char *
stun_encode32(char *buf, uint32_t data)
{
	uint32_t ndata;

	ndata = htonl(data);
	memcpy(buf, (void*)&ndata, sizeof(uint32_t));
	return (buf + sizeof(uint32_t));
}

static char *
stun_encode(char *buf, const char *data, uint32_t length)
{

	memcpy(buf, data, length);
	return (buf + length);
}

static char *
stun_encode_attr_address4(char* ptr, uint16_t type,
    const struct stun_attr_addr4 *atr)
{

	ptr = stun_encode16(ptr, type);
	ptr = stun_encode16(ptr, 8);
	*ptr++ = atr->pad;
	*ptr++ = STUN_A_IPV4FAMILY;
	ptr = stun_encode16(ptr, atr->ipv4.port);
	ptr = stun_encode32(ptr, atr->ipv4.addr);	
	return (ptr);
}

static char *
stun_encode_attr_changerequest(char* ptr,
    const struct stun_attr_changerequest *atr)
{

	ptr = stun_encode16(ptr, STUN_R_CHANGEREQUEST);
	ptr = stun_encode16(ptr, 4);
	ptr = stun_encode32(ptr, atr->value);
	return (ptr);
}

static char *
stun_encode_attr_error(char* ptr, const struct stun_attr_error *atr)
{

	ptr = stun_encode16(ptr, STUN_R_ERRORCODE);
	ptr = stun_encode16(ptr, 6 + atr->size_reason);
	ptr = stun_encode16(ptr, atr->pad);
	*ptr++ = atr->error_class;
	*ptr++ = atr->number;
	ptr = stun_encode(ptr, atr->reason, atr->size_reason);
	return (ptr);
}


static char * 
stun_encode_attr_unknown(char* ptr, const struct stun_attr_unknown *atr)
{
	int i;

	ptr = stun_encode16(ptr, STUN_R_UNKNOWNATTRIBUTE);
	ptr = stun_encode16(ptr, 2 + 2 * atr->num_attributes);
	for (i = 0; i < atr->num_attributes; i++)
		ptr = stun_encode16(ptr, atr->attr_type[i]);
	return (ptr);
}


static char * 
stun_encode_xoronly(char* ptr)
{

	ptr = stun_encode16(ptr, STUN_R_XORONLY);
	return (ptr);
}

static char *
stun_encode_attr_string(char* ptr, uint16_t type,
    const struct stun_attr_string *atr)
{

	assert(atr->size_value % 4 == 0);
	
	ptr = stun_encode16(ptr, type);
	ptr = stun_encode16(ptr, atr->size_value);
	ptr = stun_encode(ptr, atr->value, atr->size_value);
	return (ptr);
}

#if 0
static char *
stun_encode_attr_integrity(char* ptr, const struct stun_attr_integrity *atr)
{

	ptr = stun_encode16(ptr, STUN_R_MESSAGEINTEGRITY);
	ptr = stun_encode16(ptr, 20);
	ptr = stun_encode(ptr, atr->hash, sizeof(atr->hash));
	return ptr;
}
#endif

static int
stun_random_port(void)
{
	int min = 0x4000;
	int max = 0x7FFF;
	int ret;

	ret = rand();
	ret = ret | min;
	ret = ret & max;
	return (ret);
}

static void
stun_buildreq(struct stun_msg *msg,
    const struct stun_attr_string *username, int changePort, int changeIp,
    unsigned int id)
{
	int i, r;

	assert(msg != NULL);

	memset(msg, 0, sizeof(*msg));
	msg->msg_hdr.msg_type = STUN_T_BINDREQUESTMSG;
	
	for (i = 0; i < 16; i = i + 4 ) {
		assert(i + 3 < 16);
		r = rand();
		msg->msg_hdr.id.octet[i + 0]= r >> 0;
		msg->msg_hdr.id.octet[i + 1]= r >> 8;
		msg->msg_hdr.id.octet[i + 2]= r >> 16;
		msg->msg_hdr.id.octet[i + 3]= r >> 24;
	}
	if (id != 0) {
		msg->msg_hdr.id.octet[0] = id; 
	}
	msg->has_change_request = 1;
	msg->change_request.value = 0;
	if (changeIp)
		msg->change_request.value |= STUN_F_CHANGEIP;
	if (changePort)
		msg->change_request.value |= STUN_F_CHANGEPORT;
	if (username->size_value > 0) {
		msg->has_username = 1;
		msg->username = *username;
	}
}

static uint32_t
stun_encodemsg(const struct stun_msg *msg, char *buf, uint32_t buflen, 
    const struct stun_attr_string *password)
{
	char *ptr = buf;
	char *lengthp;

	assert(buflen >= sizeof(struct stun_msghdr));
	
	ptr = stun_encode16(ptr, msg->msg_hdr.msg_type);
	lengthp = ptr;
	ptr = stun_encode16(ptr, 0);
	ptr = stun_encode(ptr, (const char*)msg->msg_hdr.id.octet,
	    sizeof(msg->msg_hdr.id));
	
	if (msg->has_mapped_address) {
		ptr = stun_encode_attr_address4(ptr, STUN_R_MAPPEDADDRESS,
		    &msg->mapped_address);
	}
	if (msg->has_response_address) {
		ptr = stun_encode_attr_address4(ptr, STUN_R_RESPONSEADDRESS,
		    &msg->response_address);
	}
	if (msg->has_change_request) {
		ptr = stun_encode_attr_changerequest(ptr, &msg->change_request);
	}
	if (msg->has_source_address) {
		ptr = stun_encode_attr_address4(ptr, STUN_R_SOURCEADDRESS,
		    &msg->source_address);
	}
	if (msg->has_changed_address) {
		ptr = stun_encode_attr_address4(ptr, STUN_R_CHANGEDADDRESS,
		    &msg->changed_address);
	}
	if (msg->has_username) {
		ptr = stun_encode_attr_string(ptr, STUN_R_USERNAME,
		    &msg->username);
	}
	if (msg->has_password) {
		ptr = stun_encode_attr_string(ptr, STUN_R_PASSWORD,
		    &msg->password);
	}
	if (msg->has_error_code) {
		ptr = stun_encode_attr_error(ptr, &msg->error_code);
	}
	if (msg->has_unknown_attributes) {
		ptr = stun_encode_attr_unknown(ptr, &msg->unknown_attributes);
	}
	if (msg->has_reflected_from) {
		ptr = stun_encode_attr_address4(ptr, STUN_R_REFLECTEDFROM,
		    &msg->reflectedFrom);
	}
	if (msg->has_xor_mapped_address) {
		ptr = stun_encode_attr_address4(ptr, STUN_R_XORMAPPEDADDRESS,
		    &msg->xor_mapped_address);
	}
	if (msg->xor_only) {
		ptr = stun_encode_xoronly(ptr);
	}
	if (msg->has_server_name) {
		ptr = stun_encode_attr_string(ptr, STUN_R_SERVERNAME,
		    &msg->serverName);
	}
	if (password->size_value > 0) {
#if 0
		struct stun_attr_integrity integrity;

		stun_compute_hmac(integrity.hash, buf, (int)(ptr - buf),
		    password.value, password.size_value);
		ptr = stun_encode_attr_integrity(ptr, integrity);
#else
		assert(0 == 1);
#endif
	}
	stun_encode16(lengthp,
	    (uint16_t)(ptr - buf - sizeof(struct stun_msghdr)));
	return ((int)(ptr - buf));
}

static int
stun_sendmsg(int fd, char* buf, int l, uint32_t addr, uint16_t port)
{
	struct sockaddr_in to;
	int s, tolen = sizeof(to);

	assert(fd != -1);
	assert(addr != 0);
	assert(port != 0);
        
	memset(&to, 0, tolen);
	to.sin_family = AF_INET;
	to.sin_port = htons(port);
	to.sin_addr.s_addr = htonl(addr);
        
	s = sendto(fd, buf, l, 0,(struct sockaddr *)&to, tolen);
	if (s == -1) {
		switch (errno) {
		case ECONNREFUSED:
#if defined(__linux__)
		case EHOSTDOWN:
#endif
		case EHOSTUNREACH:
			break;
		default:
			vtc_log(stunc_vl, 0, "sendto(2) failed: %d %s",
			    errno, strerror(errno));
			break;
		}
		return (-1);
	}
	assert(s != 0);
	assert(s == l);
	return (0);;
}

static int
stun_open_port(unsigned short port, unsigned int interfaceIp)
{
	struct sockaddr_in addr;
	int fd;
    
	fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (fd == -1)
		return (-1);    
	memset((char *)&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (interfaceIp != 0 && interfaceIp != 0x100007f)
		addr.sin_addr.s_addr = htonl(interfaceIp);
	addr.sin_port = htons(port);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		ODR_close(fd);
		return (-1);
	}
	assert(fd >= 0);
	return (fd);
}

static void 
stun_sendtest(int fd, struct stun_addr4 *dest, 
    const struct stun_attr_string *username,
    const struct stun_attr_string *password, 
    int test_num)
{ 
	struct stun_msg req;
	int change_port = 0, change_ip = 0, len;
	char buf[STUN_MAX_MESSAGE_SIZE];

	assert(dest->addr != 0);
	assert(dest->port != 0);
	
	switch (test_num) {
	case 1:
	case 10:
	case 11:
		break;
	case 2:
		change_ip = 1;
		break;
	case 3:
		change_port = 1;
		break;
	default:
		assert(0 == 1);
	}
	memset(&req, 0, sizeof(req));
	stun_buildreq(&req, username, change_port, change_ip, test_num);
	len = stun_encodemsg(&req, buf, sizeof(buf), password);
	stun_sendmsg(fd, buf, len, dest->addr, dest->port);
}

static int 
stun_recvmsg(int fd, char *buf, int *len, uint32_t *src_ip, uint16_t *src_port)
{
	struct sockaddr_in from;
	int orig_size = *len;
	int fromLen = sizeof(from);

	assert(fd >= 0);
	assert(orig_size > 0);
   	
	*len = ODR_recvfrom(stunc_vl, fd, buf, orig_size, 0,
	    (struct sockaddr *)&from, &fromLen);
	if (*len == -1) {
		vtc_log(stunc_vl, 0, "recvfrom(2) failed: %d %s", errno,
		    strerror(errno));
		return (-1);
	}
	if (*len < 0)
		return (-1);
	if (*len == 0)
		return (-1);
	*src_port = ntohs(from.sin_port);
	*src_ip = ntohl(from.sin_addr.s_addr);
	if ((*len) + 1 >= orig_size)
		return (-1);
	buf[*len] = 0;
	return (0);
}

static int 
stun_parse_attr_address(char *body, unsigned int hdrlen,
    struct stun_attr_addr4 *result)
{

	if (hdrlen != 8)
		return (-1);
	result->pad = *body++;
	result->family = *body++;
	if (result->family == STUN_A_IPV4FAMILY) {
		uint32_t naddr;
		uint16_t nport;

		memcpy(&nport, body, 2);
		body += 2;
		result->ipv4.port = ntohs(nport);
		memcpy(&naddr, body, 4);
		body += 4;
		result->ipv4.addr = ntohl(naddr);

		return (0);
	}
	assert(0 == 1);
	return (-1);
}

static int
stun_parse_attr_changerequest(char *body, unsigned int hdrlen,
    struct stun_attr_changerequest *result)
{

	if (hdrlen != 4) {
		return (-1);
	}
	memcpy(&result->value, body, 4);
	result->value = ntohl(result->value);
	return (0);
}

static int
stun_parse_attr_error(char *body, unsigned int hdrlen,
    struct stun_attr_error *result)
{

	if (hdrlen >= sizeof(*result)) {
		return (-1);
	}
	memcpy(&result->pad, body, 2); body+=2;
	result->pad = ntohs(result->pad);
	result->error_class = *body++;
	result->number = *body++;
	result->size_reason = hdrlen - 4;
	memcpy(&result->reason, body, result->size_reason);
	result->reason[result->size_reason] = 0;
	return (0);
}

static int
stun_parse_attr_unknown(char* body, unsigned int hdrlen,
    struct stun_attr_unknown *result)
{
	int i;

	if (hdrlen >= sizeof(*result))
		return (-1);
	if (hdrlen % 4 != 0)
		return (-1);
	result->num_attributes = hdrlen / 4;
	for (i = 0; i < result->num_attributes; i++) {
		memcpy(&result->attr_type[i], body, 2); body+=2;
		result->attr_type[i] = ntohs(result->attr_type[i]);
	}
	return (0);
}


static int
stun_parse_attr_string(char* body, unsigned int hdrlen,
    struct stun_attr_string *result)
{

	if (hdrlen >= STUN_MAX_STRING)
		return (-1);
	if (hdrlen % 4 != 0)
		return (-1);
	result->size_value = hdrlen;
	memcpy(&result->value, body, hdrlen);
	result->value[hdrlen] = 0;
	return (0);
}

static int
stun_parse_attr_integrity(char* body, unsigned int hdrlen,
    struct stun_attr_integrity *result)
{

	if (hdrlen != 20)
		return (-1);
	memcpy(&result->hash, body, hdrlen);
	return (0);
}

static int
stun_parsemsg(char *buf, unsigned int buflen, struct stun_msg *msg)
{
	unsigned int size;
	char* body;

	if (buflen < sizeof(struct stun_msghdr))
		return (-1);
	memset(msg, 0, sizeof(*msg));
	memcpy(&msg->msg_hdr, buf, sizeof(struct stun_msghdr));
	msg->msg_hdr.msg_type = ntohs(msg->msg_hdr.msg_type);
	msg->msg_hdr.msg_length = ntohs(msg->msg_hdr.msg_length);
	if (msg->msg_hdr.msg_length + sizeof(struct stun_msghdr) != buflen)
		return (-1);
	body = buf + sizeof(struct stun_msghdr);
	size = msg->msg_hdr.msg_length;
	while (size > 0) {
		struct stun_attr_hdr *attr = (struct stun_attr_hdr*)body;
		unsigned int attrLen = ntohs(attr->length);
		int r, atrType = ntohs(attr->type);
		
		if (attrLen + 4 > size)
			return (-1);
		body += 4;
		size -= 4;
		
		switch (atrType) {
		case STUN_R_MAPPEDADDRESS:
			r = stun_parse_attr_address(body, attrLen,
			    &msg->mapped_address);
			if (r == -1)
				return (-1);
			msg->has_mapped_address = 1;
			break;  
		case STUN_R_RESPONSEADDRESS:
			r = stun_parse_attr_address(body, attrLen,
			    &msg->response_address);
			if (r == -1)
				return (-1);
			msg->has_response_address = 1;
			break;  
		case STUN_R_CHANGEREQUEST:
			r = stun_parse_attr_changerequest(body, attrLen,
			    &msg->change_request);
			if (r == -1)
				return (-1);
			msg->has_change_request = 1;
			break;
		case STUN_R_SOURCEADDRESS:
			r = stun_parse_attr_address(body, attrLen,
			    &msg->source_address);
			if (r == -1)
				return (-1);
			msg->has_source_address = 1;
			break;  
		case STUN_R_CHANGEDADDRESS:
			r = stun_parse_attr_address(body, attrLen,
			    &msg->changed_address);
			if (r == -1 )
				return (-1);
			msg->has_changed_address = 1;
			break;  
		case STUN_R_USERNAME: 
			r = stun_parse_attr_string(body, attrLen,
			    &msg->username);
			if (r == -1)
				return (-1);
			msg->has_username = 1;
			break;
		case STUN_R_PASSWORD: 
			r = stun_parse_attr_string(body, attrLen,
			    &msg->password);
			if (r == -1)
				return (-1);
			msg->has_password = 1;
			break;
		case STUN_R_MESSAGEINTEGRITY:
			r = stun_parse_attr_integrity(body, attrLen,
			    &msg->message_integrity);
			if (r == -1)
				return (-1);
			msg->has_message_integrity = 1;
			break;
		case STUN_R_ERRORCODE:
			r = stun_parse_attr_error(body, attrLen,
			    &msg->error_code);
			if (r == -1)
				return (-1);
			msg->has_error_code = 1;
			break;
		case STUN_R_UNKNOWNATTRIBUTE:
			msg->has_unknown_attributes = 1;
			r = stun_parse_attr_unknown(body, attrLen,
			    &msg->unknown_attributes);
			if (r == -1)
				return (-1);
			msg->has_unknown_attributes = 1;
			break;
		case STUN_R_REFLECTEDFROM:
			r = stun_parse_attr_address(body, attrLen,
			    &msg->reflectedFrom);
			if (r == -1)
				return (-1);
			msg->has_reflected_from = 1;
			break;  
		case STUN_R_XORMAPPEDADDRESS:
			r = stun_parse_attr_address(body, attrLen,
			    &msg->xor_mapped_address);
			if (r == -1)
				return (-1);
			msg->has_xor_mapped_address = 1;
			break;  
		case STUN_R_XORONLY:
			msg->xor_only = 1;
			break;  
		case STUN_R_SERVERNAME: 
			r = stun_parse_attr_string( body, attrLen,
			    &msg->serverName);
			if (r == -1)
				return (-1);
			msg->has_server_name = 1;
			break;
		default:
			if (atrType <= 0x7FFF) 
				return (-1);
		}
		body += attrLen;
		size -= attrLen;
	}
    
	return (0);
}

static enum stun_sm_return
stun_sm_first(struct stun_client *sc)
{
	const struct stun_addr4 *src = &sc->src;
	const struct stun_addr4 *dst = &sc->dst;

	assert(src != NULL);
	assert(dst != NULL);
	assert(dst->addr != 0);
	assert(dst->port != 0);

	sc->step = STUN_STEP_TEST_I_PREPARE;
	return (STUN_SM_RETURN_CONTINUE);
}

static enum stun_sm_return
stun_sm_test_i_prepare(struct stun_client *sc)
{
	const struct stun_addr4 *src = &sc->src;

	sc->fd = stun_open_port(src->port, src->addr);
	if (sc->fd == -1) {
		sc->step = STUN_STEP_ERROR;
		return (STUN_SM_RETURN_CONTINUE);
	}
	sc->step = STUN_STEP_TEST_I_SEND;
	return (STUN_SM_RETURN_CONTINUE);
}

static enum stun_sm_return
stun_sm_test_i_send(struct stun_client *sc)
{

	assert(sc->fd >= 0);

	stun_sendtest(sc->fd, &sc->dst, &sc->username, &sc->password, 1);

	sc->step = STUN_STEP_TEST_I_RECV;
	return (STUN_SM_RETURN_WAIT);
}

static enum stun_sm_return
stun_sm_test_i_recv(struct stun_client *sc)
{
	const struct stun_addr4 *src = &sc->src;
	struct stun_addr4 from;
	struct stun_msg resp;
	char msg[STUN_MAX_MESSAGE_SIZE];
	int msgLen = sizeof(msg), r, s;

	stun_recvmsg(sc->fd, msg, &msgLen, &from.addr, &from.port);
	memset(&resp, 0, sizeof(struct stun_msg));
	r = stun_parsemsg(msg, msgLen, &resp);
	if (r == -1)
		vtc_log(stunc_vl, 0, "stun_parsemsg() failed.");

	assert(resp.msg_hdr.id.octet[0] == 1);

	sc->test_i.mapped_addr.addr = resp.mapped_address.ipv4.addr;
	sc->test_i.mapped_addr.port = resp.mapped_address.ipv4.port;
	sc->result.preserve_port = (sc->test_i.mapped_addr.port == src->port);

	s = stun_open_port(0, sc->test_i.mapped_addr.addr);
	if (s != -1 ) {
		sc->result.is_nat = 0;
		ODR_close(s);
	} else {
		sc->result.is_nat = 1;
	}

	sc->test_i2.dst = sc->dst;
	sc->test_i2.dst.addr = resp.changed_address.ipv4.addr;

	/* Test I completed at this moment.  Move to the next. */
	sc->result.test_i_success = 1;

	sc->step = STUN_STEP_TEST_I2_PREPARE;
	return (STUN_SM_RETURN_CONTINUE);
}

static enum stun_sm_return
stun_sm_test_i2_prepare(struct stun_client *sc)
{
	struct stun_addr4 *dst = &sc->test_i2.dst;
	
	if (dst->addr == 0 || dst->port == 0) {
		sc->step = STUN_STEP_TEST_II_PREPARE;
		return (STUN_SM_RETURN_CONTINUE);
	}
	sc->step = STUN_STEP_TEST_I2_SEND;
	return (STUN_SM_RETURN_CONTINUE);
}

static enum stun_sm_return
stun_sm_test_i2_send(struct stun_client *sc)
{

	stun_sendtest(sc->fd, &sc->test_i2.dst, &sc->username, &sc->password,
	    10);
	sc->step = STUN_STEP_TEST_I2_RECV;
	return (STUN_SM_RETURN_WAIT);
}

static enum stun_sm_return
stun_sm_test_i2_recv(struct stun_client *sc)
{
	struct stun_addr4 from;
	struct stun_addr4 mapped_addr;
	struct stun_msg resp;
	char msg[STUN_MAX_MESSAGE_SIZE];
	int msgLen = sizeof(msg), r;

	stun_recvmsg(sc->fd, msg, &msgLen, &from.addr, &from.port);
	memset(&resp, 0, sizeof(struct stun_msg));
	r = stun_parsemsg(msg, msgLen, &resp);
	if (r == -1)
		vtc_log(stunc_vl, 0, "stun_parsemsg() failed.");

	assert(resp.msg_hdr.id.octet[0] == 10);

	mapped_addr.addr = resp.mapped_address.ipv4.addr;
	mapped_addr.port = resp.mapped_address.ipv4.port;
	if ((mapped_addr.addr == sc->test_i.mapped_addr.addr) &&
	    (mapped_addr.port == sc->test_i.mapped_addr.port))
		sc->result.mapped_same_ip = 1;

	/* Test I2 completed.  Move to the next */
	sc->result.test_i2_success = 1;

	sc->step = STUN_STEP_TEST_I3_PREPARE;
	return (STUN_SM_RETURN_CONTINUE);
}

static enum stun_sm_return
stun_sm_test_i3_prepare(struct stun_client *sc)
{
	struct stun_addr4 *dst = &sc->test_i.mapped_addr;
	
	if (dst->addr == 0 || dst->port == 0) {
		sc->step = STUN_STEP_TEST_II_PREPARE;
		return (STUN_SM_RETURN_CONTINUE);
	}
	sc->step = STUN_STEP_TEST_I3_SEND;
	return (STUN_SM_RETURN_CONTINUE);
}

static enum stun_sm_return
stun_sm_test_i3_send(struct stun_client *sc)
{
	struct stun_addr4 *dst = &sc->test_i.mapped_addr;

	stun_sendtest(sc->fd, dst, &sc->username, &sc->password, 11);
	sc->step = STUN_STEP_TEST_I3_RECV;
	return (STUN_SM_RETURN_WAIT);
}

static enum stun_sm_return
stun_sm_test_i3_recv(struct stun_client *sc)
{
	struct stun_addr4 from;
	struct stun_msg resp;
	char msg[STUN_MAX_MESSAGE_SIZE];
	int msgLen = sizeof(msg), r;

	stun_recvmsg(sc->fd, msg, &msgLen, &from.addr, &from.port);
	memset(&resp, 0, sizeof(struct stun_msg));
	r = stun_parsemsg(msg, msgLen, &resp);
	if (r == -1)
		vtc_log(stunc_vl, 0, "stun_parsemsg() failed.");

	assert(resp.msg_hdr.id.octet[0] == 11);

	/* Test I3 completed.  Move to the next */
	sc->result.test_i3_success = 1;
	sc->result.hairpin = 1;

	sc->step = STUN_STEP_TEST_II_PREPARE;
	return (STUN_SM_RETURN_CONTINUE);
}

static enum stun_sm_return
stun_sm_test_ii_prepare(struct stun_client *sc)
{
	struct stun_addr4 *src = &sc->src;

	if (sc->fd >= 0)
		ODR_close(sc->fd);

	sc->fd = stun_open_port(src->port + 1, src->addr);
	if (sc->fd == -1) {
		sc->step = STUN_STEP_ERROR;
		return (STUN_SM_RETURN_CONTINUE);
	}
	sc->step = STUN_STEP_TEST_II_SEND;
	return (STUN_SM_RETURN_CONTINUE);
}

static enum stun_sm_return
stun_sm_test_ii_send(struct stun_client *sc)
{
	struct stun_addr4 *dst = &sc->dst;

	stun_sendtest(sc->fd, dst, &sc->username, &sc->password, 2);

	sc->step = STUN_STEP_TEST_II_RECV;
	return (STUN_SM_RETURN_WAIT);
}

static enum stun_sm_return
stun_sm_test_ii_recv(struct stun_client *sc)
{
	struct stun_addr4 from;
	struct stun_msg resp;
	char msg[STUN_MAX_MESSAGE_SIZE];
	int msgLen = sizeof(msg), r;

	stun_recvmsg(sc->fd, msg, &msgLen, &from.addr, &from.port);
	memset(&resp, 0, sizeof(struct stun_msg));
	r = stun_parsemsg(msg, msgLen, &resp);
	if (r == -1)
		vtc_log(stunc_vl, 0, "stun_parsemsg() failed.");

	assert(resp.msg_hdr.id.octet[0] == 2);

	/* Test II completed.  Move to the next */
	if (sc->dst.addr == from.addr)
		sc->result.test_ii_fail_no_ip_change = 1;
	else
		sc->result.test_ii_success = 1;

	sc->step = STUN_STEP_TEST_III_PREPARE;
	return (STUN_SM_RETURN_CONTINUE);
}

static enum stun_sm_return
stun_sm_test_iii_prepare(struct stun_client *sc)
{

	sc->step = STUN_STEP_TEST_III_SEND;
	return (STUN_SM_RETURN_CONTINUE);
}

static enum stun_sm_return
stun_sm_test_iii_send(struct stun_client *sc)
{
	struct stun_addr4 *dst = &sc->dst;

	stun_sendtest(sc->fd, dst, &sc->username, &sc->password, 3);

	sc->step = STUN_STEP_TEST_III_RECV;
	return (STUN_SM_RETURN_WAIT);
}

static enum stun_sm_return
stun_sm_test_iii_recv(struct stun_client *sc)
{
	struct stun_addr4 from;
	struct stun_msg resp;
	char msg[STUN_MAX_MESSAGE_SIZE];
	int msglen = sizeof(msg), r;

	stun_recvmsg(sc->fd, msg, &msglen, &from.addr, &from.port);
	memset(&resp, 0, sizeof(struct stun_msg));
	r = stun_parsemsg(msg, msglen, &resp);
	if (r == -1)
		vtc_log(stunc_vl, 0, "stun_parsemsg() failed.");

	assert(resp.msg_hdr.id.octet[0] == 3);

	/* Test III completed.  Done. */
	if (sc->dst.port == from.port)
		sc->result.test_iii_fail_no_port_change = 1;
	else
		sc->result.test_iii_success = 1;

	sc->step = STUN_STEP_DONE;
	return (STUN_SM_RETURN_CONTINUE);
}

static enum stun_sm_return
stun_sm_error(struct stun_client *sc)
{

	vtc_log(stunc_vl, 0, "error");
	sc->step = STUN_STEP_DONE;
	return (STUN_SM_RETURN_CONTINUE);
}

static enum stun_sm_return
stun_sm_timeout(struct stun_client *sc)
{

	switch (sc->step_previous) {
	case STUN_STEP_TEST_I_RECV:
		/* Nothing we can do.  Might be the port isn't listening? */
		break;
	case STUN_STEP_TEST_I2_RECV:
		/* Test I2 timed out.  Move to Test II. */
		sc->step = STUN_STEP_TEST_II_PREPARE;
		return (STUN_SM_RETURN_CONTINUE);
		break;
	case STUN_STEP_TEST_I3_RECV:
		/* Test I3 timed out.  Move to Test II. */
		sc->step = STUN_STEP_TEST_II_PREPARE;
		return (STUN_SM_RETURN_CONTINUE);
	case STUN_STEP_TEST_II_RECV:
		/* Test II timed out.  Move to Test III. */
		sc->step = STUN_STEP_TEST_III_PREPARE;
		return (STUN_SM_RETURN_CONTINUE);
	case STUN_STEP_TEST_III_RECV:
		/* No more tests.  Done. */
		break;
	default:
		assert(0 == 1);
	}
	sc->step = STUN_STEP_DONE;
	return (STUN_SM_RETURN_CONTINUE);
}

static enum stun_sm_return
stun_sm_done(struct stun_client *sc)
{

	if (sc->fd >= 0)
		ODR_close(sc->fd);

#if 0
	vtc_log(stunc_vl, 2, "test_i_success %d", sc->result.test_i_success);
	vtc_log(stunc_vl, 2, "test_i2_success %d", sc->result.test_i2_success);
	vtc_log(stunc_vl, 2, "test_i3_success %d", sc->result.test_i3_success);
	vtc_log(stunc_vl, 2, "test_ii_success %d", sc->result.test_ii_success);
	vtc_log(stunc_vl, 2, "test_ii_fail_no_ip_change %d",
	    sc->result.test_ii_fail_no_ip_change);
	vtc_log(stunc_vl, 2, "test_iii_success %d", sc->result.test_iii_success);
	vtc_log(stunc_vl, 2, "test_iii_fail_no_port_change %d",
	    sc->result.test_iii_fail_no_port_change);
	vtc_log(stunc_vl, 2, "is_nat %d", sc->result.is_nat);
	vtc_log(stunc_vl, 2, "preserve_port %d", sc->result.preserve_port);
	vtc_log(stunc_vl, 2, "hairpin %d", sc->result.hairpin);
	vtc_log(stunc_vl, 2, "mapped_same_ip %d", sc->result.mapped_same_ip);
#endif

	return (STUN_SM_RETURN_ABORT);
}

static enum stun_sm_return
stun_client_sm(struct stun_client *sc)
{
	enum stun_sm_return r = STUN_SM_RETURN_CONTINUE;

	while (r == STUN_SM_RETURN_CONTINUE) {
		switch (sc->step) {
		case STUN_STEP_FIRST:
			r = stun_sm_first(sc);
			break;
		case STUN_STEP_TEST_I_PREPARE:
			r = stun_sm_test_i_prepare(sc);
			break;
		case STUN_STEP_TEST_I_SEND:
			r = stun_sm_test_i_send(sc);
			break;
		case STUN_STEP_TEST_I_RECV:
			r = stun_sm_test_i_recv(sc);
			break;
		case STUN_STEP_TEST_I2_PREPARE:
			r = stun_sm_test_i2_prepare(sc);
			break;
		case STUN_STEP_TEST_I2_SEND:
			r = stun_sm_test_i2_send(sc);
			break;
		case STUN_STEP_TEST_I2_RECV:
			r = stun_sm_test_i2_recv(sc);
			break;
		case STUN_STEP_TEST_I3_PREPARE:
			r = stun_sm_test_i3_prepare(sc);
			break;
		case STUN_STEP_TEST_I3_SEND:
			r = stun_sm_test_i3_send(sc);
			break;
		case STUN_STEP_TEST_I3_RECV:
			r = stun_sm_test_i3_recv(sc);
			break;
		case STUN_STEP_TEST_II_PREPARE:
			r = stun_sm_test_ii_prepare(sc);
			break;
		case STUN_STEP_TEST_II_SEND:
			r = stun_sm_test_ii_send(sc);
			break;
		case STUN_STEP_TEST_II_RECV:
			r = stun_sm_test_ii_recv(sc);
			break;
		case STUN_STEP_TEST_III_PREPARE:
			r = stun_sm_test_iii_prepare(sc);
			break;
		case STUN_STEP_TEST_III_SEND:
			r = stun_sm_test_iii_send(sc);
			break;
		case STUN_STEP_TEST_III_RECV:
			r = stun_sm_test_iii_recv(sc);
			break;
		case STUN_STEP_ERROR:
			r = stun_sm_error(sc);
			break;
		case STUN_STEP_TIMEOUT:
			r = stun_sm_timeout(sc);
			break;
		case STUN_STEP_DONE:
			r = stun_sm_done(sc);
			break;
		default:
			assert(0 == 1);
		}
	}
	return (r);
}

static void
stun_client_wait(struct stun_client *sc)
{
	struct timeval tv;
	fd_set set;
	int err;

	FD_ZERO(&set);
	FD_SET(sc->fd, &set);
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	err = select(sc->fd + 1, &set, NULL, NULL, &tv);
	if (err == -1) {
		sc->step = STUN_STEP_ERROR;
		return;
	} else if (err == 0) {
		sc->step_previous = sc->step;
		sc->step = STUN_STEP_TIMEOUT;
		return;
	}
	if (!FD_ISSET(sc->fd, &set))
		assert(0 == 1);
}

void
stun_client_perform(struct stun_client *sc)
{
	enum stun_sm_return r;

	assert(sc->step == STUN_STEP_FIRST);
	assert(sc->fd == -1);

	while (sc->step != STUN_STEP_DONE) {
		r = stun_client_sm(sc);
		if (r == STUN_SM_RETURN_WAIT) {
			stun_client_wait(sc);
		}
	}
}

const char *
STUNC_nattypestr(enum stun_nattype t)
{

	switch (t) {
	case STUN_NATTYPE_FAILURE:
		return ("FAILURE");
	case STUN_NATTYPE_BLOCKED:
		return ("BLOCKED");
	case STUN_NATTYPE_FULL_CONE:
		return ("FULL_CONE");
	case STUN_NATTYPE_RESTRICTED_CONE:
		return ("RESTRICTED_CONE");
	case STUN_NATTYPE_PORT_RESTRICTED_CONE:
		return ("PORT_RESTRICTED_CONE");
	case STUN_NATTYPE_SYMMETRIC:
		return ("SYMMETRIC");
	case STUN_NATTYPE_OPEN:
		return ("OPEN");
	case STUN_NATTYPE_FIREWALL:
		return ("FIREWALL");
	case STUN_NATTYPE_UNKNOWN:
		return ("UNKNOWN");
	default:
		break;
	}
	return ("UNEXPECTED");
}

enum stun_nattype
stun_client_get_nattype(struct stun_client *sc)
{

	if (sc->result.test_ii_fail_no_ip_change ||
	    sc->result.test_iii_fail_no_port_change) {
		/*
		 * If we're here, it means something is wrong while performing
		 * STUN tests. We can't determine the NAT type.
		 */
		return (STUN_NATTYPE_FAILURE);
	}
	if (!sc->result.test_i_success)
		return (STUN_NATTYPE_BLOCKED);
	if (sc->result.is_nat) {
		if (sc->result.mapped_same_ip) {
			if (sc->result.test_ii_success)
				return (STUN_NATTYPE_FULL_CONE);
			if (sc->result.test_iii_success)
				return (STUN_NATTYPE_RESTRICTED_CONE);
			return (STUN_NATTYPE_PORT_RESTRICTED_CONE);
		}
		return (STUN_NATTYPE_SYMMETRIC);
	}
	if (sc->result.test_ii_success)
		return (STUN_NATTYPE_OPEN);
	return (STUN_NATTYPE_FIREWALL);
}

struct stun_addr4
stun_client_get_mapped_addr(struct stun_client *sc)
{
	struct stun_addr4 error = { 0, 0 };

	if (!sc->result.test_i_success)
		return (error);
	return (sc->test_i.mapped_addr);
}

enum stun_nattype
STUNC_get_nattype(void)
{

	assert(stunc_result_inited == 1);
	return (stunc_result.nattype);
}

const char *
STUNC_get_mappped_addr(void)
{
	struct in_addr addr;

	assert(stunc_result_inited == 1);
	addr.s_addr = stunc_result.mapped_addr;
	return (inet_ntoa(addr));
}

int
STUNC_test(void)
{
	struct stun_addr4 mapped_addr;
	struct stun_client sc;
	enum stun_nattype nattype;
	struct in_addr in;

	vtc_log(stunc_vl, 2,
	    "Starting to test the STUN client. It takes some seconds.");

	memset(&sc, 0, sizeof(sc));
	sc.step = STUN_STEP_FIRST;
	sc.src.addr = 0;
	sc.src.port = stun_random_port();
	sc.dst.addr = ntohl(inet_addr("13.56.166.106"));
	sc.dst.port = 3478;
	sc.fd = -1;
	stun_client_perform(&sc);

	nattype = stun_client_get_nattype(&sc);
	mapped_addr = stun_client_get_mapped_addr(&sc);

	in.s_addr = htonl(mapped_addr.addr);
	vtc_log(stunc_vl, 2,
	    "STUN client test completed. (nat_type %s mapped_addr %s)",
	    STUNC_nattypestr(nattype),
	    inet_ntoa(in));

	if (stunc_result_inited) {
		uint32_t naddr = htonl(mapped_addr.addr);

		if (stunc_result.nattype != nattype) {
			vtc_log(stunc_vl, 2, "NAT type changed from %d to %d",
			    stunc_result.nattype, nattype);
			stunc_result.nattype = nattype;
		}
		if (stunc_result.mapped_addr != naddr) {
			struct in_addr addr1, addr2;
			const char *addr1p, *addr2p;
			char addr1b[64], addr2b[64];

			addr1.s_addr = stunc_result.mapped_addr;
			addr2.s_addr = naddr;
			addr1p = inet_ntop(AF_INET, &addr1, addr1b,
			    sizeof(addr1b));
			addr2p = inet_ntop(AF_INET, &addr2, addr2b,
			    sizeof(addr2b));
			vtc_log(stunc_vl, 2,
			    "Mapped address changed from %s to %s",
			    addr1p, addr2p);
			stunc_result.mapped_addr = naddr;
		}
	} else {
		stunc_result.nattype = nattype;
		stunc_result.mapped_addr = htonl(mapped_addr.addr);
		stunc_result_inited = 1;
	}
	return (0);
}

int
STUNC_init(void)
{

	stunc_vl = vtc_logopen("stunc", mudband_log_printf);
	AN(stunc_vl);
	STUNC_test();
	return (0);
}
