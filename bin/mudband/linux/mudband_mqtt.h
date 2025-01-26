#if !defined(__MQTT_H__)
#define __MQTT_H__

#include <limits.h>
#include <stdint.h>

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

#define MQTT_PROTOCOL_LEVEL 0x04

#define MQTT_GENERATE_ENUM(ENUM) ENUM,

#define MQTT_ALL_ERRORS(MQTT_ERROR)				\
	MQTT_ERROR(MQTT_ERROR_NULLPTR)				\
	MQTT_ERROR(MQTT_ERROR_CONTROL_FORBIDDEN_TYPE)		\
	MQTT_ERROR(MQTT_ERROR_CONTROL_INVALID_FLAGS)		\
	MQTT_ERROR(MQTT_ERROR_CONTROL_WRONG_TYPE)		\
	MQTT_ERROR(MQTT_ERROR_CONNECT_CLIENT_ID_REFUSED)	\
	MQTT_ERROR(MQTT_ERROR_CONNECT_NULL_WILL_MESSAGE)	\
	MQTT_ERROR(MQTT_ERROR_CONNECT_FORBIDDEN_WILL_QOS)	\
	MQTT_ERROR(MQTT_ERROR_CONNACK_FORBIDDEN_FLAGS)		\
	MQTT_ERROR(MQTT_ERROR_CONNACK_FORBIDDEN_CODE)		\
	MQTT_ERROR(MQTT_ERROR_PUBLISH_FORBIDDEN_QOS)		\
	MQTT_ERROR(MQTT_ERROR_SUBSCRIBE_TOO_MANY_TOPICS)	\
	MQTT_ERROR(MQTT_ERROR_MALFORMED_RESPONSE)		\
	MQTT_ERROR(MQTT_ERROR_UNSUBSCRIBE_TOO_MANY_TOPICS)	\
	MQTT_ERROR(MQTT_ERROR_RESPONSE_INVALID_CONTROL_TYPE)	\
	MQTT_ERROR(MQTT_ERROR_CONNECT_NOT_CALLED)		\
	MQTT_ERROR(MQTT_ERROR_SEND_BUFFER_IS_FULL)		\
	MQTT_ERROR(MQTT_ERROR_SOCKET_ERROR)			\
	MQTT_ERROR(MQTT_ERROR_MALFORMED_REQUEST)		\
	MQTT_ERROR(MQTT_ERROR_RECV_BUFFER_TOO_SMALL)		\
	MQTT_ERROR(MQTT_ERROR_ACK_OF_UNKNOWN)			\
	MQTT_ERROR(MQTT_ERROR_NOT_IMPLEMENTED)			\
	MQTT_ERROR(MQTT_ERROR_CONNECTION_REFUSED)		\
	MQTT_ERROR(MQTT_ERROR_SUBSCRIBE_FAILED)			\
	MQTT_ERROR(MQTT_ERROR_CONNECTION_CLOSED)		\
	MQTT_ERROR(MQTT_ERROR_INITIAL_RECONNECT)		\
	MQTT_ERROR(MQTT_ERROR_INVALID_REMAINING_LENGTH)		\
	MQTT_ERROR(MQTT_ERROR_CLEAN_SESSION_IS_REQUIRED)	\
	MQTT_ERROR(MQTT_ERROR_RECONNECTING)

enum mqtt_error {
	MQTT_ERROR_UNKNOWN=INT_MIN,
	MQTT_ALL_ERRORS(MQTT_GENERATE_ENUM)
	MQTT_OK = 1
};

enum mqtt_publish_flags {
	MQTT_PUBLISH_DUP = 8u,
	MQTT_PUBLISH_QOS_0 = ((0u << 1) & 0x06),
	MQTT_PUBLISH_QOS_1 = ((1u << 1) & 0x06),
	MQTT_PUBLISH_QOS_2 = ((2u << 1) & 0x06),
	MQTT_PUBLISH_QOS_MASK = ((3u << 1) & 0x06),
	MQTT_PUBLISH_RETAIN = 0x01
};

enum mqtt_queued_message_state {
	MQTT_QUEUED_UNSENT,
	MQTT_QUEUED_AWAITING_ACK,
	MQTT_QUEUED_COMPLETE
};

enum mqtt_control_packet_type {
	MQTT_CONTROL_CONNECT=1u,
	MQTT_CONTROL_CONNACK=2u,
	MQTT_CONTROL_PUBLISH=3u,
	MQTT_CONTROL_PUBACK=4u,
	MQTT_CONTROL_PUBREC=5u,
	MQTT_CONTROL_PUBREL=6u,
	MQTT_CONTROL_PUBCOMP=7u,
	MQTT_CONTROL_SUBSCRIBE=8u,
	MQTT_CONTROL_SUBACK=9u,
	MQTT_CONTROL_UNSUBSCRIBE=10u,
	MQTT_CONTROL_UNSUBACK=11u,
	MQTT_CONTROL_PINGREQ=12u,
	MQTT_CONTROL_PINGRESP=13u,
	MQTT_CONTROL_DISCONNECT=14u
};

enum mqtt_connect_flags {
	MQTT_CONNECT_RESERVED = 1u,
	MQTT_CONNECT_CLEAN_SESSION = 2u,
	MQTT_CONNECT_WILL_FLAG = 4u,
	MQTT_CONNECT_WILL_QOS_0 = (0u & 0x03) << 3,
	MQTT_CONNECT_WILL_QOS_1 = (1u & 0x03) << 3,
	MQTT_CONNECT_WILL_QOS_2 = (2u & 0x03) << 3,
	MQTT_CONNECT_WILL_RETAIN = 32u,
	MQTT_CONNECT_PASSWORD = 64u,
	MQTT_CONNECT_USER_NAME = 128u
};

struct mqtt_queued_message {
	uint8_t		*start;
	size_t		size;
	enum mqtt_queued_message_state state;
	time_t		time_sent;
	enum mqtt_control_packet_type control_type;
	uint16_t	packet_id;
};

struct mqtt_message_queue {
	void		*mem_start;
	void		*mem_end;
	uint8_t		*curr;
	size_t		curr_sz;
	struct mqtt_queued_message *queue_tail;
};

struct mqtt_fixed_header {
	enum mqtt_control_packet_type control_type;
	uint32_t	control_flags: 4;
	uint32_t	remaining_length;
};

enum mqtt_connack_return_code {
	MQTT_CONNACK_ACCEPTED = 0u,
	MQTT_CONNACK_REFUSED_PROTOCOL_VERSION = 1u,
	MQTT_CONNACK_REFUSED_IDENTIFIER_REJECTED = 2u,
	MQTT_CONNACK_REFUSED_SERVER_UNAVAILABLE = 3u,
	MQTT_CONNACK_REFUSED_BAD_USER_NAME_OR_PASSWORD = 4u,
	MQTT_CONNACK_REFUSED_NOT_AUTHORIZED = 5u
};

struct mqtt_response_connack {
	uint8_t		session_present_flag;
	enum mqtt_connack_return_code return_code;
};

struct mqtt_response_publish {
	uint8_t		dup_flag;
	uint8_t		qos_level;
	uint8_t		retain_flag;
	uint16_t	topic_name_size;
	const void	*topic_name;
	uint16_t	packet_id;
	const void	*application_message;
	size_t		application_message_size;
};
struct mqtt_response_puback {
	uint16_t	packet_id;
};

struct mqtt_response_pubrec {
	uint16_t	packet_id;
};

struct mqtt_response_pubrel {
	uint16_t	packet_id;
};

struct mqtt_response_pubcomp {
	uint16_t	packet_id;
};

struct mqtt_response_suback {
	uint16_t	packet_id;
	const uint8_t	*return_codes;
	size_t		num_return_codes;
};
struct mqtt_response_unsuback {
	uint16_t	packet_id;
};
struct mqtt_response_pingresp {
	int		dummy;
};

struct mqtt_response {
	struct mqtt_fixed_header fixed_header;
	union {
		struct mqtt_response_connack  connack;
		struct mqtt_response_publish  publish;
		struct mqtt_response_puback   puback;
		struct mqtt_response_pubrec   pubrec;
		struct mqtt_response_pubrel   pubrel;
		struct mqtt_response_pubcomp  pubcomp;
		struct mqtt_response_suback   suback;
		struct mqtt_response_unsuback unsuback;
		struct mqtt_response_pingresp pingresp;
	} decoded;
};

struct mqtt_client {
	int		socketfd;
	uint16_t	pid_lfsr;
	uint16_t	keep_alive;
	int		number_of_keep_alives;
	size_t		send_offset;
	time_t		time_of_last_send;
	enum mqtt_error	error;
	int		response_timeout;
	int		number_of_timeouts;
	double		typical_response_time;
	void		(*publish_response_callback)(void** state,
			    struct mqtt_response_publish *publish);
	void		*publish_response_callback_state;
	enum mqtt_error	(*inspector_callback)(struct mqtt_client*);
	void		(*reconnect_callback)(struct mqtt_client*, void**);
	void		*reconnect_state;
	struct {
		uint8_t	*mem_start;
		size_t	mem_size;
		uint8_t	*curr;
		size_t	curr_sz;
	} recv_buffer;
	struct mqtt_message_queue mq;
};

int	MQTT_init(void);
void	MQTT_sync(void);
void	MQTT_subscribe(void);

#endif
