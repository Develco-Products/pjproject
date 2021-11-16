#ifndef _PJSUA_SSAPMSG_H_
#define _PJSUA_SSAPMSG_H_

enum ssapmsg_type {
	SSAPMSG_UNDEFINED	= 0,
	SSAPMSG_CALL			= 1,
	SSAPMSG_SCAIPMSG	= 2,
	SSAPMSG_CONFIG		= 3,
	SSAPMSG_PJSUA			= 4,
	SSAPMSG_NO_OF_MSG_TYPES,
	SSAPMSG_ACK				= 0xA0,
	SSAPMSG_NACK			= 0xA1,
	SSAPMSG_WARNING		= 0xFE,
	SSAPMSG_ERROR			= 0xFF,
};

// TODO: Should I have response codes in a seperate enum?:q

#define SSAPMSG_PAYLOAD_BUFFER_MAX (256)

/* Payload - Used to interpret incomming message. */
struct ssapmsg_scaipmsg {
	uint16_t msg_offset;
	char data[0];
};

/*
 * Wrapper for a legacy text-based command passed on to pjsua's text 
 * interface. 
 * NOTE: This has NO protection against ending in a dead-end where the 
 * program waits for more text based input. Use with care.
 */
struct ssapmsg_pjsua {
	/* Must be a null-terminated string. */
	uint8_t data[0];
};

struct ssapmsg_response {
	pj_status_t code;
	char* msg[0];
};

union ssapmsg_payload {
	struct ssapmsg_scaipmsg scaipmsg;
	struct ssapmsg_pjsua pjsua;
	struct ssapmsg_response response;
	uint8_t raw[SSAPMSG_PAYLOAD_BUFFER_MAX];	// Max payload buffer.
};

struct ssapmsg_datagram {
	uint32_t ref;
	enum ssapmsg_type type;
	uint32_t payload_size;
	uint32_t crc;
	union ssapmsg_payload payload;
};


/* Interface to received data for all types. */
struct ssapmsg_iface {
	uint32_t ref;
	enum ssapmsg_type type;
	union {
		// scaip message
		struct {
			char* address;
			char* msg;
		};
		// pjsua
		struct {
			char* data;
		};
	};
};


#define SSAPMSG_HEADER_SIZE (sizeof(struct ssapmsg_datagram) - sizeof(union ssapmsg_payload))

char* ssapmsgtype_str(enum ssapmsg_type type);
pj_status_t ssapmsg_parse(struct ssapmsg_iface* dst, struct ssapmsg_datagram* msg, pj_ssize_t msg_size);
void ssapmsg_print(const struct ssapmsg_datagram* const msg);
void update_crc32(struct ssapmsg_datagram* const ssapmsg);
pj_status_t validate_ssapmsg_crc(struct ssapmsg_datagram* const ssapmsg);


pj_status_t ssapmsg_receive(struct ssapmsg_datagram* const msg);
pj_ssize_t ssapmsg_send(struct ssapmsg_datagram* const msg);

//TODO: Should you implement these? Do pjsua ever need to send these to SSAP?
//pj_ssize_t ssapmsg_send_call(const struct ssapmsg_call* const msg);
//pj_ssize_t ssapmsg_send_config(const struct ssapmsg_config* const msg);
//pj_ssize_t ssapmsg_send_pjsua_response(const struct ssapmsg_pjsua* const msg);
pj_status_t ssapmsg_response_send(const pj_status_t resp, const char* const message);
pj_ssize_t ssapmsg_scaipmsg_send(const char* const addr, const char* const msg);

#endif /* _PJSUA_SSAPMSG_H_ */

