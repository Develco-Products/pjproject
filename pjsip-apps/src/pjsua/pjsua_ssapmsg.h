#ifndef _PJSUA_SSAPMSG_H_
#define _PJSUA_SSAPMSG_H_

#include "pjsua_app_common.h"


#define SSAPMSG_PROTOCOL_VERSION (0)
#define SSAPMSG_PAYLOAD_BUFFER_MAX (256)

#define SSAPMSG_HEADER_SIZE (sizeof(ssapmsg_datagram_t) - sizeof(union ssapmsg_payload))
#define SSAPMSG_MAX_PAYLOAD_SIZE (256)
#define SSAPMSG_VARDATA_MAX_SIZE(msg_type) (SSAPMSG_MAX_PAYLOAD_SIZE - sizeof(msg_type))

typedef enum __attribute__((__packed__)) {
	SSAPMSG_UNDEFINED	= 0,
	SSAPMSG_ACK,
	SSAPMSG_NACK,
	SSAPMSG_WARNING,
	SSAPMSG_ERROR,

	SSAPCALL_DIAL			= 0x1000,
	SSAPCALL_ANSWER,
	SSAPCALL_HANGUP,
	SSAPCALL_STATUS,
	SSAPCALL_MIC_SENSITIVITY,
	SSAPCALL_MIC_SENSITIVITY_INFO,
	SSAPCALL_SPEAKER_VOLUME,
	SSAPCALL_SPEAKER_VOLUME_INFO,
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
	SSAPCONFIG_ACCOUNT_LIST,
	SSAPCONFIG_ACCOUNT_INFO,
	SSAPCONFIG_BUDDY_ADD,
	SSAPCONFIG_BUDDY_DELETE,
	SSAPCONFIG_BUDDY_LIST,
	SSAPCONFIG_BUDDY_INFO,

	SSAPPJSUA					= 0x4000,
} ssapmsg_t;

#define SSAPMSG_STATUS_REFERENCE 					(-1)
#define SSAPMSG_NO_RESPONSE_PENDING				(-2)
#define SSAPMSG_MESSAGE_FLOW_OUT_OF_SYNC 	(-3)

/* For switches
	  case SSAPMSG_UNDEFINED:
	  case SSAPMSG_ACK:
	  case SSAPMSG_NACK:
	  case SSAPMSG_WARNING:
	  case SSAPMSG_ERROR:

	  case SSAPCALL_DIAL:
		case SSAPCALL_ANSWER:
	  case SSAPCALL_HANGUP:
		case SSAPCALL_STATUS:
	  case SSAPCALL_MIC_SENSITIVITY:
		case SSAPCALL_MIC_SENSITIVITY_INFO:
	  case SSAPCALL_SPEAKER_VOLUME:
		case SSAPCALL_SPEAKER_VOLUME_INFO:
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
		case SSAPCONFIG_ACCOUNT_LIST:
		case SSAPCONFIG_ACCOUNT_INFO:
		case SSAPCONFIG_BUDDY_ADD:
		case SSAPCONFIG_BUDDY_DELETE:
		case SSAPCONFIG_BUDDY_INFO:
		case SSAPCONFIG_BUDDY_INFO:

	  case SSAPPJSUA:
		*/


/* Payload - Used to interpret incomming message. */
typedef struct __attribute__((__packed__)) {
	uint8_t data[0];
} ssap_no_payload_t;

/* Generalized wrapper for no defined data structure. */
typedef struct __attribute__((__packed__)) {
	/* Must be a null-terminated string. */
	uint8_t data[0];
} ssapmsg_raw_t;



typedef struct {
	pj_status_t code;
	pj_str_t msg;
} ssapmsg_status_t;

typedef struct __attribute__((__packed__)) {
	int32_t code;
	char msg[0];
} ssapmsg_status_payload_t;

typedef struct {
	pjsua_call_id id;
	pjsua_acc_id acc_id;
	pj_str_t sip_address;
} ssapcall_dial_t;

typedef struct __attribute__((__packed__)) {
	int16_t id;
	int16_t acc_id;
	char sip_address[0];
} ssapcall_dial_payload_t;

/* ssapcall_status_t:
 * Use a partially initialized pjsua_call_info. */

typedef struct __attribute__((__packed__)) {
	uint16_t id;
	uint16_t role;
	uint16_t acc_id;
	uint16_t state;
	char state_text[0];
} ssapcall_status_payload_t;

typedef struct {
	float sensitivity;
	pj_bool_t mute;
} ssapcall_micsensitivity_t;

typedef struct __attribute__((__packed__)) {
	uint16_t sensitivity;
	uint16_t mute;
} ssapcall_micsensitivity_payload_t;

/* ssapcall_micsensitivityinfo_t
 * does not have an internal type. */

typedef ssap_no_payload_t ssapcall_micsensitivityinfo_payload_t;

typedef struct {
	float volume;
	pj_bool_t mute;
} ssapcall_speakervolume_t;

typedef struct __attribute__((__packed__)) {
	uint16_t volume;
	uint16_t mute;
} ssapcall_speakervolume_payload_t;

/* ssapcall_speakervolumeinfo_t
 * does not have an internal type. */

typedef ssap_no_payload_t ssapcall_speakervolumeinfo_payload_t;

typedef struct {
	pj_str_t receiver_url;
	pj_str_t msg;
} ssapmsg_scaip_t;

typedef struct __attribute__((__packed__)) {
	uint16_t msg_offset;
	char data[0];
} ssapmsg_scaip_payload_t;


typedef struct {
	pj_str_t sip_address;
	pj_str_t registrar;
	pj_str_t auth_realm;
	pj_str_t auth_user;
	pj_str_t auth_pass;
} ssapconfig_account_add_t;

typedef struct __attribute__((__packed__)) {
	//uint16_t sip_url_offset;	// implicit 0.
	uint16_t registrar_offset;
	uint16_t auth_realm_offset;
	uint16_t auth_user_offset;
	uint16_t auth_pass_offset;
	char data[0];
} ssapconfig_account_add_payload_t;

typedef struct {
	pjsua_acc_id id;
} ssapconfig_account_del_t;

typedef struct __attribute__((__packed__)) {
	uint16_t id;
} ssapconfig_account_del_payload_t;

typedef struct {
	unsigned int acc_no;
	pjsua_acc_id acc[PJSUA_MAX_ACC];
} ssapconfig_account_list_t;

typedef struct __attribute__((__packed__)) {
	uint16_t acc_no;
	uint16_t acc[0];
} ssapconfig_account_list_payload_t;

/* ssapconfig_account_info_t:
 * Use a partially initialized pjsua_acc_info. */

typedef struct __attribute__((__packed__)) {
	uint16_t id;
	uint32_t reg_code;
	uint32_t reg_expires;
	uint32_t reg_last_error;
	uint16_t is_default_acc;
	uint16_t is_online;
	char sip_address[0];
} ssapconfig_account_info_payload_t;

typedef struct {
	pj_str_t sip_url;
}	ssapconfig_buddy_add_t;

typedef struct __attribute__((__packed__)) {
	char sip_url[0];
}	ssapconfig_buddy_add_payload_t;

typedef struct {
	pjsua_buddy_id id;
}	ssapconfig_buddy_del_t;

typedef struct __attribute__((__packed__)) {
	uint16_t id;
}	ssapconfig_buddy_del_payload_t;

typedef struct {
	unsigned int buddy_no;
	pjsua_buddy_id buddy[PJSUA_MAX_BUDDIES];
} ssapconfig_buddy_list_t;

typedef struct __attribute__((__packed__)) {
	uint16_t buddy_no;
	uint16_t buddy[0];
} ssapconfig_buddy_list_payload_t;

/* ssapconfig_buddy_info_t:
 * Use a partially initialized pjsua_buddy_info */

typedef struct __attribute__((__packed__)) {
	uint16_t id;
	char sip_url[0];
}	ssapconfig_buddy_info_payload_t;


typedef struct {
	pj_str_t pjsua_cmd;
} ssappjsua_pjsua_t;

typedef struct __attribute__((__packed__)) {
	char pjsua_cmd[0];
} ssappjsua_pjsua_payload_t;

/*
 * Wrapper for a legacy text-based command passed on to pjsua's text 
 * interface. 
 * NOTE: This has NO protection against ending in a dead-end where the 
 * program waits for more text based input. Use with care.
 */

union ssapmsg_payload {
	ssap_no_payload_t undefined;
	ssapmsg_status_payload_t ack;
	ssapmsg_status_payload_t nack;
	ssapmsg_status_payload_t warning;
	ssapmsg_status_payload_t error;

	ssapcall_dial_payload_t call_dial;
	ssap_no_payload_t call_hangup;
	ssapcall_status_payload_t call_status;
	ssapcall_micsensitivity_payload_t call_mic_sensitivity;
	ssapcall_micsensitivityinfo_payload_t call_mic_sensitivity_info;
	ssapcall_speakervolume_payload_t call_speaker_volume;
	ssapcall_speakervolumeinfo_payload_t call_speaker_volume_info;
	ssap_no_payload_t call_dtmf;
	ssap_no_payload_t call_quality;
	ssap_no_payload_t call_info;

	//ssapmsg_scaip_t msg_scaip;
	ssapmsg_scaip_payload_t msg_scaip;
	ssap_no_payload_t msg_dtmf;
	ssap_no_payload_t msg_plain;

	ssap_no_payload_t config_quit;
	ssap_no_payload_t config_reload;
	ssap_no_payload_t config_status;
	ssapconfig_account_add_payload_t config_account_add;
	ssapconfig_account_del_payload_t config_account_del;
	ssapconfig_account_list_payload_t config_account_list;
	ssapconfig_account_info_payload_t config_account_info;
	ssapconfig_buddy_add_payload_t config_buddy_add;
	ssapconfig_buddy_del_payload_t config_buddy_del;
	ssapconfig_buddy_list_payload_t config_buddy_list;
	ssapconfig_buddy_info_payload_t config_buddy_info;

	ssappjsua_pjsua_payload_t pjsua;
	uint8_t raw[SSAPMSG_PAYLOAD_BUFFER_MAX];	// Max payload buffer.
};

typedef struct __attribute__((__packed__)) {
	int16_t ref;
	uint16_t protocol_version;
	ssapmsg_t type;
	uint16_t payload_size;
	uint32_t crc;
	uint8_t data[0];
	union ssapmsg_payload payload;
} ssapmsg_datagram_t;


PJ_INLINE(pj_str_t) pj_packstr(const char* const str) {
	return pj_str((char*) str);
}

PJ_INLINE(ssapmsg_t) ssapmsg_status(pj_status_t s) {
	return ((s == PJ_SUCCESS) ? SSAPMSG_ACK : SSAPMSG_NACK);
}

PJ_INLINE(pj_ssize_t) ssapmsg_datagram_size(const ssapmsg_datagram_t* const msg) {
	return SSAPMSG_HEADER_SIZE + msg->payload_size;
}

PJ_INLINE(void) ssapmsg_clone(ssapmsg_datagram_t* const dst, const ssapmsg_datagram_t* const src) {
	pj_memcpy((void*) dst, src, ssapmsg_datagram_size(src));
}

PJ_INLINE(pj_bool_t) reference_is_valid(int16_t ref) {
	return (ref >= 0) && (ref <= INT16_MAX);
}

PJ_INLINE(pj_bool_t) ssapmsg_is_status(const ssapmsg_datagram_t* const d) {
	return d->ref == SSAPMSG_STATUS_REFERENCE;
}

pj_ssize_t pjstr_extract(char* const dst, const pj_str_t* const pj_str, const ssize_t max_length);

char* ssapmsgtype_str(ssapmsg_t type);
void ssapmsg_print(ssapmsg_datagram_t* const msg);

void update_crc32(ssapmsg_datagram_t* const ssapmsg);
pj_status_t validate_ssapmsg_crc(ssapmsg_datagram_t* const ssapmsg);

pj_status_t ssapmsg_receive(ssapmsg_datagram_t* const msg);
pj_status_t ssapmsg_unpack(void* const ssapmsg, const ssapmsg_datagram_t* const datagram);
pj_status_t ssapmsg_pack(ssapmsg_datagram_t* const datagram, const void* const payload);

pj_ssize_t ssapmsg_send(ssapmsg_datagram_t* const msg);
pj_status_t ssapmsg_send_request(const ssapmsg_t type, const void* const payload);
pj_status_t ssapmsg_send_reply(const ssapmsg_t type, const void* const payload);
pj_status_t ssapmsg_send_status(const ssapmsg_t type, const void* const payload);
pj_status_t ssapmsg_response(const pj_status_t status, const char* const msg);

#endif /* _PJSUA_SSAPMSG_H_ */

