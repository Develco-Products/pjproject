#ifndef _PJSUA_SSAPMSG_H_
#define _PJSUA_SSAPMSG_H_

#define SSAPMSG_PROTOCOL_VERSION (0)

typedef enum __attribute__((__packed__)) {
	SSAPMSG_UNDEFINED	= 0,
	SSAPMSG_ACK,
	SSAPMSG_NACK,
	SSAPMSG_WARNING,
	SSAPMSG_ERROR,

	SSAPCALL_DIAL			= 0x1000,
	SSAPCALL_HANGUP,
	SSAPCALL_STATUS,
	SSAPCALL_MIC_SENSITIVITY,
	SSAPCALL_MUTE_MIC,
	SSAPCALL_SPEAKER_VOLUME,
	SSAPCALL_MUTE_SPEAKER,
	SSAPCALL_DTMF,
	SSAPCALL_QUALITY,
	SSAPCALL_INFO,

	SSAPMSG_SCAIP			= 0x2000,
	SSAPMSG_DTMF,
	SSAPMSG_PLAIN,

	SSAPCONFIG_QUIT		= 0x3000,
	SSAPCONFIG_RELOAD,
	SSAPCONFIG_STATUS,
	SSAPCONFIG_ACCOUNT_ADD,
	SSAPCONFIG_ACCOUNT_DELETE,
	SSAPCONFIG_ACCOUNT_INFO,
	SSAPCONFIG_BUDDY_ADD,
	SSAPCONFIG_BUDDY_DELETE,
	SSAPCONFIG_BUDDY_INFO,

	SSAPPJSUA					= 0x4000,
} ssapmsg_t;

#define SSAPMSG_STATUS_REFERENCE 					(-1)
#define SSAPMSG_MESSAGE_FLOW_OUT_OF_SYNC 	(-2)

/* For switches
	  case SSAPMSG_UNDEFINED:
	  case SSAPMSG_ACK:
	  case SSAPMSG_NACK:
	  case SSAPMSG_WARNING:
	  case SSAPMSG_ERROR:

	  case SSAPCALL_DIAL:
	  case SSAPCALL_HANGUP:
		case SSAPCALL_STATUS:
	  case SSAPCALL_MIC_SENSITIVITY:
		case SSAPCALL_MUTE_MIC:
	  case SSAPCALL_SPEAKER_VOLUME:
		case SSAPCALL_MUTE_SPEAKER:
	  case SSAPCALL_DTMF:
	  case SSAPCALL_QUALITY:
	  case SSAPCALL_INFO:

	  case SSAPMSG_SCAIP:
		case SSAPMSG_DTMF:
	  case SSAPMSG_PLAIN:

	  case SSAPCONFIG_QUIT:
	  case SSAPCONFIG_RELOAD:
	  case SSAPCONFIG_STATUS:
		case SSAPCONFIG_ACCOUNT_ADD:
		case SSAPCONFIG_ACCOUNT_DELETE:
		case SSAPCONFIG_ACCOUNT_INFO:
		case SSAPCONFIG_BUDDY_ADD:
		case SSAPCONFIG_BUDDY_DELETE:
		case SSAPCONFIG_BUDDY_INFO:

	  case SSAPPJSUA:
		*/

// TODO: Should I have response codes in a seperate enum?:q

#define SSAPMSG_PAYLOAD_BUFFER_MAX (256)

/* Payload - Used to interpret incomming message. */
typedef struct __attribute__((__packed__)) {
	uint8_t data[0];
} ssap_no_payload_t;

/* Generalized wrapper for no defined data structure. */
typedef struct __attribute__((__packed__)) {
	/* Must be a null-terminated string. */
	uint8_t data[0];
} ssapmsg_raw_t;

typedef struct __attribute__((__packed__)) {
	char sip_address[0];
} ssapcall_dial_t;

typedef struct __attribute__((__packed__)) {
	uint16_t id;
	uint16_t state;
	char state_text[0];
} ssapcall_status_t;

typedef struct __attribute__((__packed__)) {
	uint16_t msg_offset;
	char data[0];
} ssapmsg_scaip_t;

typedef struct __attribute__((__packed__)) {
	uint16_t buddy_id;
	char* sip_address[0];
}	ssapconfig_buddy_info;

/*
 * Wrapper for a legacy text-based command passed on to pjsua's text 
 * interface. 
 * NOTE: This has NO protection against ending in a dead-end where the 
 * program waits for more text based input. Use with care.
 */

typedef struct __attribute__((__packed__)) {
	pj_status_t code;
	char* msg[0];
} ssapmsg_status_t;

union ssapmsg_payload {
	ssap_no_payload_t undefined;
	ssapmsg_status_t ack;
	ssapmsg_status_t nack;
	ssapmsg_status_t warning;
	ssapmsg_status_t error;

	ssapcall_dial_t call_dial;
	ssap_no_payload_t call_hangup;
	ssapcall_status_t call_status;
	ssap_no_payload_t call_mic_sensitivity;
	ssap_no_payload_t call_mute_mic;
	ssap_no_payload_t call_speaker_volume;
	ssap_no_payload_t call_mute_speaker;
	ssap_no_payload_t call_dtmf;
	ssap_no_payload_t call_quality;
	ssap_no_payload_t call_info;

	ssapmsg_scaip_t msg_scaip;
	ssap_no_payload_t msg_dtmf;
	ssap_no_payload_t msg_plain;

	ssap_no_payload_t config_quit;
	ssap_no_payload_t config_reload;
	ssap_no_payload_t config_status;
	ssap_no_payload_t config_account_add;
	ssap_no_payload_t config_account_del;
	ssap_no_payload_t config_account_info;
	ssapconfig_buddy_info config_buddy_add;
	ssapconfig_buddy_info config_buddy_del;
	ssapconfig_buddy_info config_buddy_info;

	ssapmsg_raw_t pjsua;
	uint8_t raw[SSAPMSG_PAYLOAD_BUFFER_MAX];	// Max payload buffer.
};

typedef struct __attribute__((__packed__)) {
	int16_t ref;
	uint16_t protocol_version;
	ssapmsg_t type;
	uint16_t payload_size;
	uint32_t crc;
	union ssapmsg_payload payload;
} ssapmsg_datagram_t;


#if 0
/* Interface to received data for all types. */
struct ssapmsg_iface {
	uint16_t ref;
	ssapmsg_t type;
	union {
		struct
		// msg_scaip
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
#endif


#define SSAPMSG_HEADER_SIZE (sizeof(ssapmsg_datagram_t) - sizeof(union ssapmsg_payload))
#define SSAPMSG_MAX_PAYLOAD_SIZE (256)
#define SSAPMSG_VARDATA_SIZE(msg_type) (SSAPMSG_MAX_PAYLOAD_SIZE - sizeof(msg_type))

void pjstr_extract(char* const dst, const pj_str_t* const pj_str, const ssize_t max_length);

char* ssapmsgtype_str(ssapmsg_t type);
//pj_status_t ssapmsg_parse(struct ssapmsg_iface* dst, ssapmsg_datagram_t* datagram);
void ssapmsg_print(ssapmsg_datagram_t* const msg);
void update_crc32(ssapmsg_datagram_t* const ssapmsg);
pj_status_t validate_ssapmsg_crc(ssapmsg_datagram_t* const ssapmsg);

void ssapmsg_clone(ssapmsg_datagram_t* const dst, const ssapmsg_datagram_t* const src);
pj_bool_t ssapmsg_is_status(const ssapmsg_datagram_t* const d);

pj_ssize_t ssapmsg_datagram_size(const ssapmsg_datagram_t* const msg);
const char* const ssapmsg_scaip_address(const ssapmsg_datagram_t* msg);
const char* const ssapmsg_scaip_message(const ssapmsg_datagram_t* msg);


pj_status_t ssapmsg_receive(ssapmsg_datagram_t* const msg);
pj_ssize_t ssapmsg_send(ssapmsg_datagram_t* const msg);
pj_status_t ssapmsg_response_send(const pj_status_t status, const char* const msg);

//TODO: Should you implement these? Do pjsua ever need to send these to SSAP?
//pj_ssize_t ssapmsg_send_call(const struct ssapmsg_call* const msg);
//pj_ssize_t ssapmsg_send_config(const struct ssapmsg_config* const msg);
//pj_ssize_t ssapmsg_send_pjsua_response(const struct ssapmsg_pjsua* const msg);
pj_status_t ssapcall_dial_send(const char* const addr);
pj_status_t ssapcall_status_send(const pjsua_call_info* const call_state);
pj_status_t ssapmsg_scaip_send(const char* const addr, const char* const msg);

#endif /* _PJSUA_SSAPMSG_H_ */

