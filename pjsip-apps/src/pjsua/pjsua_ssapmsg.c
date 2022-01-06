#include <pjsua-lib/pjsua.h>
#include "pjsua_app_common.h"

#include "pjsua_ssapmsg.h"
#include "pjlib.h"
#include "pjsua_app.h"
#include <string.h>
#include "pj/types.h"
#include "pjlib-util/crc32.h"
#include "pjsua_ssapcomm.h"
#include "pjsua_ssaperr.h"

#define THIS_FILE "pjsua_ssapmsg"

extern pj_caching_pool ssap_mem;
static pj_pool_t* rcv_pool = NULL;
static uint8_t* rcv_buffer = NULL;
static uint8_t* rcv_end = NULL;

static ssapmsg_datagram_t response_token;


static void sync_response(ssapmsg_datagram_t* const datagram);
static void reset_rcv_buffer();
static pj_status_t rcv_buffer_push(const uint8_t* const block, const pj_ssize_t bytes);
static pj_status_t rcv_buffer_shift(ssapmsg_datagram_t* const datagram);

static pj_status_t ssapmsg_fetch_data(ssapmsg_datagram_t* msg, pj_ssize_t* lim);
static pj_bool_t valid_datagram(ssapmsg_datagram_t* const d);
	
/* Make a safe conversion of pj_str to char* buffer. Including handling hidden
 * null-strings and strings with missing null-terminators. */
pj_ssize_t pjstr_extract(char* const dst, const pj_str_t* const pj_str, const ssize_t max_length) {
	const char* const s = pj_strbuf(pj_str);
	const pj_size_t pj_str_length = pj_strlen(pj_str);
	if(s == NULL || pj_str_length == 0) {
		dst[0] = '\0';
		return 1;
	}
	else {
		const pj_ssize_t cp_bytes = (pj_str_length > max_length -1) 
			? max_length -1 
			: pj_str_length;
		pj_memcpy(dst, s, cp_bytes);
		dst[cp_bytes] = '\0';
		return cp_bytes +1;
	}
}

char* ssapmsgtype_str(ssapmsg_t type) {
  char* ret = NULL;

  switch(type) {
	  case SSAPMSG_UNDEFINED:
      ret = "SSAPMSG_UNDEFINED";
      break;
	  case SSAPMSG_ACK:
      ret = "SSAPMSG_ACK";
      break;
	  case SSAPMSG_NACK:
      ret = "SSAPMSG_NACK";
      break;
	  case SSAPMSG_WARNING:
      ret = "SSAPMSG_WARNING";
      break;
	  case SSAPMSG_ERROR:
      ret = "SSAPMSG_ERROR";
      break;

	  case SSAPCALL_DIAL:
	  	ret = "SSAPCALL_DIAL";
      break;
	  case SSAPCALL_ANSWER:
	  	ret = "SSAPCALL_ANSWER";
      break;
	  case SSAPCALL_HANGUP:
	  	ret = "SSAPCALL_HANGUP";
      break;
		case SSAPCALL_STATUS:
			ret = "SSAPCALL_STATUS";
			break;
	  case SSAPCALL_MIC_SENSITIVITY:
	  	ret = "SSAPCALL_MIC_SENSITIVITY";
      break;
		case SSAPCALL_MUTE_MIC:
			ret = "SSAPCALL_MUTE_MIC";
			break;
	  case SSAPCALL_SPEAKER_VOLUME:
	  	ret = "SSAPCALL_SPEAKER_VOLUME";
      break;
		case SSAPCALL_MUTE_SPEAKER:
			ret = "SSAPCALL_MUTE_SPEAKER";
			break;
	  case SSAPCALL_DTMF:
	  	ret = "SSAPCALL_DTMF";
      break;
	  case SSAPCALL_QUALITY:
	  	ret = "SSAPCALL_QUALITY";
      break;
	  case SSAPCALL_INFO:
	  	ret = "SSAPCALL_INFO";
      break;

	  case SSAPMSG_SCAIP:
	  	ret = "SSAPMSG_SCAIP";
      break;
		case SSAPMSG_DTMF:
			ret = "SSAPMSG_DTMF";
			break;
	  case SSAPMSG_PLAIN:
	  	ret = "SSAPMSG_PLAIN";
      break;

	  case SSAPCONFIG_QUIT:
	  	ret = "SSAPCONFIG_QUIT";
      break;
	  case SSAPCONFIG_RELOAD:
	  	ret = "SSAPCONFIG_RELOAD";
      break;
	  case SSAPCONFIG_STATUS:
	  	ret = "SSAPCONFIG_STATUS";
      break;
		case SSAPCONFIG_ACCOUNT_ADD:
			ret = "SSAPCONFIG_ACCOUNT_ADD";
      break;
		case SSAPCONFIG_ACCOUNT_DELETE:
			ret = "SSAPCONFIG_ACCOUNT_DELETE";
      break;
		case SSAPCONFIG_ACCOUNT_LIST:
			ret = "SSAPCONFIG_ACCOUNT_LIST";
      break;
		case SSAPCONFIG_ACCOUNT_INFO:
			ret = "SSAPCONFIG_ACCOUNT_INFO";
      break;
		case SSAPCONFIG_BUDDY_ADD:
			ret = "SSAPCONFIG_BUDDY_ADD";
      break;
		case SSAPCONFIG_BUDDY_DELETE:
			ret = "SSAPCONFIG_BUDDY_DELETE";
      break;
		case SSAPCONFIG_BUDDY_LIST:
			ret = "SSAPCONFIG_BUDDY_LIST";
      break;
		case SSAPCONFIG_BUDDY_INFO:
			ret = "SSAPCONFIG_BUDDY_INFO";
      break;

	  case SSAPPJSUA:
	  	ret = "SSAPPJSUA";
      break;

    default:
      PJ_LOG(3, (THIS_FILE, "INVALID SSAPMSG enum value: %d", type));
      ret = "";
      break;
  }

  return ret;
}

void ssapmsg_print(ssapmsg_datagram_t* const msg) {
  char buffer[1024];

  PJ_LOG(3, (THIS_FILE, "SSAP Message -------------"));
  PJ_LOG(3, (THIS_FILE, "Header"));
  PJ_LOG(3, (THIS_FILE, "  Ref          :  %d / 0x%X", msg->ref, msg->ref));
  PJ_LOG(3, (THIS_FILE, "  Version      :  %d", msg->protocol_version));
  PJ_LOG(3, (THIS_FILE, "  Type         :  0x%X (%s)", msg->type, ssapmsgtype_str(msg->type)));
  PJ_LOG(3, (THIS_FILE, "  Payload Size :  %d", msg->payload_size));
  PJ_LOG(3, (THIS_FILE, "  CRC32        :  0x%X (%s)", msg->crc, ((validate_ssapmsg_crc(msg) == PJ_SUCCESS) ? "passed" : "fail")));
	
  PJ_LOG(3, (THIS_FILE, "Payload"));
	
  switch(msg->type) {
	  case SSAPMSG_UNDEFINED:
      PJ_LOG(3, (THIS_FILE, "  - no data -"));
      break;
	  case SSAPMSG_ACK:
	  case SSAPMSG_NACK:
	  case SSAPMSG_WARNING:
	  case SSAPMSG_ERROR:
      PJ_LOG(3, (THIS_FILE, "  - not implemented -"));
      break;

	  case SSAPCALL_DIAL:
      PJ_LOG(3, (THIS_FILE, "  address      : %s", msg->payload.call_dial.sip_address));
      break;
	  case SSAPCALL_ANSWER:
	  case SSAPCALL_HANGUP:
      PJ_LOG(3, (THIS_FILE, "  - not implemented -"));
			break;
		case SSAPCALL_STATUS:
      PJ_LOG(3, (THIS_FILE, "  call id      : %d", msg->payload.call_status.id));
      PJ_LOG(3, (THIS_FILE, "  call state   : %d", msg->payload.call_status.state));
      PJ_LOG(3, (THIS_FILE, "  state text   : %s", msg->payload.call_status.state_text));
      break;
	  case SSAPCALL_MIC_SENSITIVITY:
		case SSAPCALL_MUTE_MIC:
	  case SSAPCALL_SPEAKER_VOLUME:
		case SSAPCALL_MUTE_SPEAKER:
	  case SSAPCALL_DTMF:
	  case SSAPCALL_QUALITY:
	  case SSAPCALL_INFO:
      PJ_LOG(3, (THIS_FILE, "  - not implemented -"));
			break;

	  case SSAPMSG_SCAIP:
			{
				uint16_t offset = msg->payload.msg_scaip.msg_offset;
	      PJ_LOG(3, (THIS_FILE, "  address      : %s", msg->payload.msg_scaip.data));
	      PJ_LOG(3, (THIS_FILE, "  message      : %s", msg->payload.msg_scaip.data + offset));
			}
			break;

		case SSAPMSG_DTMF:
	  case SSAPMSG_PLAIN:

	  case SSAPCONFIG_QUIT:
      PJ_LOG(3, (THIS_FILE, "  - no payload -"));
      break;
	  case SSAPCONFIG_RELOAD:
      PJ_LOG(3, (THIS_FILE, "  - no payload -"));
      break;
	  case SSAPCONFIG_STATUS:
      PJ_LOG(3, (THIS_FILE, "  - not implemented -"));
      break;
		case SSAPCONFIG_ACCOUNT_ADD:
			{
				uint16_t offset = 0;
	      PJ_LOG(3, (THIS_FILE, "  sip address  : %s", msg->payload.config_account_add.data + offset));
				offset = msg->payload.config_account_add.registrar_offset;
	      PJ_LOG(3, (THIS_FILE, "  registrar    : %s", msg->payload.config_account_add.data + offset));
				offset = msg->payload.config_account_add.auth_realm_offset;
	      PJ_LOG(3, (THIS_FILE, "  auth realm   : %s", msg->payload.config_account_add.data + offset));
				offset = msg->payload.config_account_add.auth_user_offset;
	      PJ_LOG(3, (THIS_FILE, "  username     : %s", msg->payload.config_account_add.data + offset));
				offset = msg->payload.config_account_add.auth_pass_offset;
	      PJ_LOG(3, (THIS_FILE, "  password     : %s", msg->payload.config_account_add.data + offset));
      	break;
			}
		case SSAPCONFIG_ACCOUNT_DELETE:
	    PJ_LOG(3, (THIS_FILE, "  account id   : %d", msg->payload.config_account_del.id));
			break;
		case SSAPCONFIG_ACCOUNT_LIST:
			{
				char buffer[64] = { '\0' };

				PJ_LOG(3, (THIS_FILE, "  account no   : %d", msg->payload.config_account_list.acc_no));
				for(int i=0; i<msg->payload.config_account_list.acc_no; i += 8) {
					char* p = buffer;

					for(int j=i; j<msg->payload.config_account_list.acc_no && j < i+8; ++j) {
						p += pj_utoa(msg->payload.config_account_list.acc[j], p);
						*p++ = ' ';
						*p++ = '\0';
					}

					PJ_LOG(3, (THIS_FILE, "%s", buffer));
				}
				break;
			}
		case SSAPCONFIG_ACCOUNT_INFO:
			{
	    	PJ_LOG(3, (THIS_FILE, "  account id   : %d", msg->payload.config_account_info.id));
	      PJ_LOG(3, (THIS_FILE, "  sip address  : %s", msg->payload.config_account_info.sip_address));
	    	PJ_LOG(3, (THIS_FILE, "  reg status   : %d", msg->payload.config_account_info.reg_code));
	    	PJ_LOG(3, (THIS_FILE, "  reg expires  : %d", msg->payload.config_account_info.reg_expires));
	    	PJ_LOG(3, (THIS_FILE, "  reg last err : %d", msg->payload.config_account_info.reg_last_error));
	    	PJ_LOG(3, (THIS_FILE, "  default acc  : %s", (msg->payload.config_account_info.is_default_acc) ? "yes" : "no"));
	    	PJ_LOG(3, (THIS_FILE, "  online  			: %s", (msg->payload.config_account_info.is_online) ? "yes" : "no"));
				break;
			}
		case SSAPCONFIG_BUDDY_ADD:
	    PJ_LOG(3, (THIS_FILE, "  address      : %s", msg->payload.config_buddy_add.sip_url));
			break;
		case SSAPCONFIG_BUDDY_DELETE:
	    PJ_LOG(3, (THIS_FILE, "  buddy id     : %d", msg->payload.config_buddy_del.id));
			break;
		case SSAPCONFIG_BUDDY_LIST:
			{
				char buffer[64] = { '\0' };

				PJ_LOG(3, (THIS_FILE, "  buddy no   : %d", msg->payload.config_buddy_list.buddy_no));
				for(int i=0; i<msg->payload.config_buddy_list.buddy_no; i += 8) {
					char* p = buffer;

					for(int j=i; j<msg->payload.config_buddy_list.buddy_no && j < i+8; ++j) {
						p += pj_utoa(msg->payload.config_buddy_list.buddy[j], p);
						*p++ = ' ';
						*p++ = '\0';
					}

					PJ_LOG(3, (THIS_FILE, "%s", buffer));
				}
				break;
			}
		case SSAPCONFIG_BUDDY_INFO:
	    PJ_LOG(3, (THIS_FILE, "  buddy id     : %d", msg->payload.config_buddy_info.id));
	    PJ_LOG(3, (THIS_FILE, "  address      : %s", msg->payload.config_buddy_info.sip_url));
			break;

	  case SSAPPJSUA:
      PJ_LOG(3, (THIS_FILE, "  pjsua cmd      : %s", msg->payload.pjsua.pjsua_cmd));
      break;

    default:
      PJ_LOG(3, (THIS_FILE, "  Invalid type. Raw hex dump of 64B payload:", msg->type));
			print_raw_hex(msg->payload.raw, 64);
      break;
  }
  PJ_LOG(3, (THIS_FILE, "-- End of message: %d", msg->ref));
}


void update_crc32(ssapmsg_datagram_t* const ssapmsg) {
  /* Clear out any existing crc before calculation. */
  ssapmsg->crc = 0u;

  uint32_t crc = pj_crc32_calc((uint8_t*) ssapmsg, ssapmsg->payload_size + SSAPMSG_HEADER_SIZE);
  ssapmsg->crc = crc;

  PJ_LOG(5, (THIS_FILE, "crc calculated: 0x%X", crc));
}


pj_status_t validate_ssapmsg_crc(ssapmsg_datagram_t* const ssapmsg) {
  uint32_t msg_crc = ssapmsg->crc;
  uint32_t msg_calc;

  /* Zero out crc for use in calculation. */
  ssapmsg->crc = 0u;

  /* Calculate crc for entire buffer. */
  msg_calc = pj_crc32_calc( (uint8_t*) ssapmsg, ssapmsg->payload_size + SSAPMSG_HEADER_SIZE );

  /* Restore message crc so the original message is restored. */
  ssapmsg->crc = msg_crc;

  PJ_LOG(5, (THIS_FILE, "crc validation. Received: 0x%X, Calculated: 0x%X", msg_crc, msg_calc));
  
  return (msg_calc == msg_crc)
    ? PJ_SUCCESS
    : PJ_EINVAL;
}


pj_status_t ssapmsg_receive(ssapmsg_datagram_t* const msg) {
  pj_ssize_t lim = sizeof(ssapmsg_datagram_t);
  pj_status_t res = -1, rcv_res;

 	PJ_LOG(3, (THIS_FILE, "Retrieving datagram."));
	res = ssapmsg_fetch_data(msg, &lim);

	switch(res) {
		case PJ_SUCCESS:

      /* Validate that packet has valid size value. */
      if(ssapmsg_datagram_size(msg) != lim) {
        PJ_LOG(3, (THIS_FILE, "Packet is malformed. Size: %dB, Received: %d", msg->payload_size + SSAPMSG_HEADER_SIZE, lim));
        return PJ_EINVAL;
      }

			if(valid_datagram(msg)) {
      	/* Set token for response. */
      	if(response_token.type != SSAPMSG_UNDEFINED) {
      	  PJ_LOG(3, (THIS_FILE, "Overwriting existing pending response token for: %s with ref: %d", ssapmsgtype_str(response_token.type), response_token.ref));
      	}

				if(reference_is_valid(msg->ref)) {
      		response_token.ref  = msg->ref;
      		response_token.protocol_version = msg->protocol_version;
      		response_token.type = msg->type;
      		PJ_LOG(3, (THIS_FILE, "Setting pending response for %s with ref: %d", ssapmsgtype_str(response_token.type), response_token.ref));
				}
				/* else: status message or error message. Do not track reference for a reply. */
			}
			/* else: Invalid datagram. Dropped. */
			break;

    case PJ_STATUS_FROM_OS(OSERR_ECONNRESET):
      // Connection reset by peer. Reset and listen for new connection.
			break;
    case PJ_STATUS_FROM_OS(OSERR_EAFNOSUPPORT):
    case PJ_STATUS_FROM_OS(OSERR_ENOPROTOOPT):  
    case PJ_STATUS_FROM_OS(ENOTSOCK):
    case PJ_STATUS_FROM_OS(EFAULT):
  	default:
  	  {
				PJ_PERROR(3, (THIS_FILE, res, "Resulting in Quit command by default. Reason: %d (0x%X)", res, res));
  	    const char* const cmd_quit = "q\n";
  	    const uint32_t l = strlen(cmd_quit) +1;

  	    msg->ref = SSAPMSG_STATUS_REFERENCE; 
				msg->protocol_version = SSAPMSG_PROTOCOL_VERSION;
  	    msg->type = SSAPPJSUA;
  	    msg->payload_size = l;
  	    msg->crc = 0u;  // dummy crc.
  	    pj_memcpy(msg->payload.pjsua.pjsua_cmd, cmd_quit, l);

  	    res = PJ_SUCCESS;
  	  }
  	  break;

#if 0
  	default:
  	  /* No a result which is handled here. */
  	 	PJ_LOG(3, (THIS_FILE, "Response id: %d (0x%X) is NOT handled.", res, res));
			res = PJ_STATUS_FROM_OS(res);
  	 	PJ_LOG(3, (THIS_FILE, "OS id      : %d (0x%X) is NOT handled.", res, res));
  	  break;
#endif
	}

  return res;
}

pj_status_t ssapmsg_unpack(void* const ssapmsg, const ssapmsg_datagram_t* const datagram) {
	pj_status_t res; 

	switch(datagram->type) {
	  case SSAPMSG_UNDEFINED:
			PJ_LOG(3, (THIS_FILE, "Message type (%s) cannot be unpacked.", ssapmsgtype_str(datagram->type)));
			res = PJ_EINVAL;
			break;
	  case SSAPMSG_ACK:				/* Ack, Nack, Warning and Error are all status messages 	  */
	  case SSAPMSG_NACK:			/* and can be treated equally when packing them to payload  */
	  case SSAPMSG_WARNING:		/* messages. 																							  */
	  case SSAPMSG_ERROR:			/* Ack through Warning are purposely fall-through. 				  */
			{
				ssapmsg_status_t* const dst = (ssapmsg_status_t* const) ssapmsg;
				const ssapmsg_status_payload_t* const src = &datagram->payload.ack;
				dst->code = src->code;
				pj_cstr(&dst->msg, src->msg);
				res = PJ_SUCCESS;
			}
			break;


	  case SSAPCALL_DIAL:
			{
				ssapcall_dial_t* const dst = (ssapcall_dial_t* const) ssapmsg;
				const ssapcall_dial_payload_t* const src = &datagram->payload.call_dial;
				pj_cstr(&dst->sip_address, src->sip_address);
				res = PJ_SUCCESS;
			}
			break;

	  case SSAPCALL_ANSWER:
	  case SSAPCALL_HANGUP:
			PJ_LOG(3, (THIS_FILE, "Method type (%s) not implemented.", ssapmsgtype_str(datagram->type)));
			break;
		case SSAPCALL_STATUS:
			{
				pjsua_call_info* const dst = (pjsua_call_info* const) ssapmsg;
				const ssapcall_status_payload_t* const src = &datagram->payload.call_status;

				dst->id = (pjsua_call_id) src->id;
				dst->role = (pjsip_role_e) src->role;
				dst->acc_id = (pjsua_acc_id) src->acc_id;
				dst->state = (pjsip_inv_state) src->state;
				pj_cstr(&dst->state_text, src->state_text);
				res = PJ_SUCCESS;
				break;
			}
	  case SSAPCALL_MIC_SENSITIVITY:
		case SSAPCALL_MUTE_MIC:
	  case SSAPCALL_SPEAKER_VOLUME:
		case SSAPCALL_MUTE_SPEAKER:
	  case SSAPCALL_DTMF:
	  case SSAPCALL_QUALITY:
	  case SSAPCALL_INFO:
			PJ_LOG(3, (THIS_FILE, "Method type (%s) not implemented.", ssapmsgtype_str(datagram->type)));
			res = PJ_EINVAL;
			break;


	  case SSAPMSG_SCAIP:
			{
				ssapmsg_scaip_t* const dst = (ssapmsg_scaip_t* const) ssapmsg;
				const ssapmsg_scaip_payload_t* const src = &datagram->payload.msg_scaip;
				pj_cstr(&dst->receiver_url, src->data);
				pj_cstr(&dst->msg, src->data + src->msg_offset);
				res = PJ_SUCCESS;
				break;
			}
		case SSAPMSG_DTMF:
	  case SSAPMSG_PLAIN:
			PJ_LOG(3, (THIS_FILE, "Method type (%s) not implemented.", ssapmsgtype_str(datagram->type)));
			res = PJ_EINVAL;
			break;

	  case SSAPCONFIG_QUIT:	/* fall through */
	  case SSAPCONFIG_RELOAD:
			/* No payload to be unpacked. */
			res = PJ_SUCCESS;
			break;
	  case SSAPCONFIG_STATUS:
			PJ_LOG(3, (THIS_FILE, "Method type (%s) not implemented.", ssapmsgtype_str(datagram->type)));
			break;
		case SSAPCONFIG_ACCOUNT_ADD:
			{
				ssapconfig_account_add_t* const dst = (ssapconfig_account_add_t* const) ssapmsg;
				const ssapconfig_account_add_payload_t* const src = &datagram->payload.config_account_add;
				uint16_t offset = 0;
				pj_cstr(&dst->sip_address, &src->data[offset]);
				offset = src->registrar_offset;
				pj_cstr(&dst->registrar, &src->data[offset]);
				offset = src->auth_realm_offset;
				pj_cstr(&dst->auth_realm, &src->data[offset]);
				offset = src->auth_user_offset;
				pj_cstr(&dst->auth_user, &src->data[offset]);
				offset = src->auth_pass_offset;
				pj_cstr(&dst->auth_pass, &src->data[offset]);
				res = PJ_SUCCESS;
				break;
			}
		case SSAPCONFIG_ACCOUNT_DELETE:
			((ssapconfig_account_del_t*) ssapmsg)->id = (pjsua_acc_id) datagram->payload.config_account_del.id;
			res = PJ_SUCCESS;
			break;
		case SSAPCONFIG_ACCOUNT_LIST:
			{
				ssapconfig_account_list_t* const dst = (ssapconfig_account_list_t* const) ssapmsg;
				const ssapconfig_account_list_payload_t* const src = &datagram->payload.config_account_list;

				dst->acc_no = src->acc_no;
				for(int i=0; i<dst->acc_no; ++i) {
					dst->acc[i] = (pjsua_acc_id) src->acc[i];
				}
				break;
			}
		case SSAPCONFIG_ACCOUNT_INFO:
			{
				pjsua_acc_info* const dst = (pjsua_acc_info* const) ssapmsg;
				const ssapconfig_account_info_payload_t* const src = &datagram->payload.config_account_info;
				dst->id = (pjsua_acc_id) src->id;
				dst->status = (pjsip_status_code) src->reg_code;
				dst->expires = src->reg_expires;
				dst->reg_last_err = (pj_status_t) src->reg_last_error;
				dst->is_default = (pj_bool_t) src->is_default_acc;
				dst->online_status = (pj_bool_t) src->is_online;
				pj_cstr(&dst->acc_uri, src->sip_address);
				res = PJ_SUCCESS;
				break;
			}
		case SSAPCONFIG_BUDDY_ADD:
			{
				ssapconfig_buddy_add_t* const dst = (ssapconfig_buddy_add_t* const) ssapmsg;
				const ssapconfig_buddy_add_payload_t* const src = &datagram->payload.config_buddy_add;
				pj_cstr(&dst->sip_url, src->sip_url);
				res = PJ_SUCCESS;
			}
			break;
		case SSAPCONFIG_BUDDY_DELETE:
			((ssapconfig_buddy_del_t*) ssapmsg)->id = (pjsua_buddy_id) datagram->payload.config_buddy_del.id;
			res = PJ_SUCCESS;
			break;
		case SSAPCONFIG_BUDDY_LIST:
			{
				ssapconfig_buddy_list_t* const dst = (ssapconfig_buddy_list_t* const) ssapmsg;
				const ssapconfig_buddy_list_payload_t* const src = &datagram->payload.config_buddy_list;
      	
				dst->buddy_no = src->buddy_no;
				for(int i=0; i<dst->buddy_no; ++i) {
					dst->buddy[i] = (pjsua_buddy_id) src->buddy[i];
				}
				res = PJ_SUCCESS;
				break;
			}
		case SSAPCONFIG_BUDDY_INFO:
			{
				pjsua_buddy_info* const dst = (pjsua_buddy_info* const) ssapmsg;
				const ssapconfig_buddy_info_payload_t* const src = &datagram->payload.config_buddy_info;

				dst->id = (pjsua_buddy_id) src->id;
				pj_cstr(&dst->uri, src->sip_url);
				res = PJ_SUCCESS;
				break;
			}

	  case SSAPPJSUA:
			{
				ssappjsua_pjsua_t* const dst = (ssappjsua_pjsua_t* const) ssapmsg;
				const ssappjsua_pjsua_payload_t* const src = &datagram->payload.pjsua;
				pj_cstr(&dst->pjsua_cmd, src->pjsua_cmd);
				res = PJ_SUCCESS;
				break; 
			}

		default:
			PJ_LOG(3, (THIS_FILE, "Invalid message type (0x%X) cannot be unpaced.", datagram->type));
			res = PJ_EINVAL;
			break;
	}

	return res;
}

pj_status_t ssapmsg_pack(ssapmsg_datagram_t* const datagram, const void* const payload) {
	pj_status_t res;

	PJ_LOG(3, (THIS_FILE, "Packing: %s", ssapmsgtype_str(datagram->type)));
	/* Each message type MUST set:
	 *  - datagram.payload.<type>.
	 *  - datagram.payload_length - if message has no payload, set payload_length = 0.
	 *  */
	switch(datagram->type) {
	  case SSAPMSG_UNDEFINED:
			PJ_LOG(3, (THIS_FILE, "Method type (%s) not implemented.", ssapmsgtype_str(datagram->type)));
			datagram->payload_size = 0;
			res = PJ_EINVAL;
			break;

	  case SSAPMSG_ACK:				/* Ack, Nack, Warning and Error are all status messages 	  */
	  case SSAPMSG_NACK:			/* and can be treated equally when packing them to payload  */
	  case SSAPMSG_WARNING:		/* messages. 																							  */
	  case SSAPMSG_ERROR:			/* Ack through Warning are purposely fall-through. 				  */
			{
				const ssapmsg_status_t* const status = (const ssapmsg_status_t* const) payload;
				/* Accessing fields through payload.ack. 
				 * Since Ack, Nack, Warning and Error are of the same type this is safe. */
				ssapmsg_status_payload_t* const datagram_payload = &datagram->payload.ack;

				datagram_payload->code = status->code;
				pj_ssize_t remaining_bytes = SSAPMSG_VARDATA_MAX_SIZE(ssapmsg_status_payload_t);

				if(pj_strlen(&status->msg) > remaining_bytes) {
					PJ_LOG(3, (THIS_FILE, "Payload overflow in %s message of %dB! Excessive bytes dropped!.", 
								ssapmsgtype_str(datagram->type), 
								pj_strlen(&status->msg) - remaining_bytes));
				}

				remaining_bytes -= pjstr_extract(datagram_payload->msg, &status->msg, remaining_bytes);

				datagram->payload_size = sizeof(ssapmsg_status_payload_t) +
					SSAPMSG_VARDATA_MAX_SIZE(ssapmsg_status_payload_t) - remaining_bytes;
				res = PJ_SUCCESS;
				break;
			}

	  case SSAPCALL_DIAL:
			{
				const ssapcall_dial_t* const dial = (const ssapcall_dial_t* const) payload;
				ssapcall_dial_payload_t* const datagram_payload = &datagram->payload.call_dial;

				pj_ssize_t remaining_bytes = SSAPMSG_VARDATA_MAX_SIZE(ssapcall_dial_payload_t);
				if(pj_strlen(&dial->sip_address) > remaining_bytes) {
					PJ_LOG(3, (THIS_FILE, "Payload overflow in %s message of %dB! Excessive bytes dropped!.", 
								ssapmsgtype_str(datagram->type),
								pj_strlen(&dial->sip_address) - remaining_bytes));
				}
				remaining_bytes -= pjstr_extract(datagram_payload->sip_address, &dial->sip_address, remaining_bytes);

				datagram->payload_size = sizeof(ssapcall_dial_payload_t) + 
					SSAPMSG_VARDATA_MAX_SIZE(ssapcall_dial_payload_t) - remaining_bytes;
				res = PJ_SUCCESS;
				break;
			}
	  case SSAPCALL_ANSWER:
	  case SSAPCALL_HANGUP:
			PJ_LOG(3, (THIS_FILE, "Method type (%s) not implemented.", ssapmsgtype_str(datagram->type)));
			datagram->payload_size = 0;
			res = PJ_EINVAL;
			break;
		case SSAPCALL_STATUS:
			{
				const pjsua_call_info* const status  = (const pjsua_call_info* const) payload;
				ssapcall_status_payload_t* const datagram_payload = &datagram->payload.call_status;

				datagram_payload->id = (uint16_t) status->id;
				datagram_payload->role = (uint16_t) status->role;
				datagram_payload->acc_id = (uint16_t) status->acc_id;
				datagram_payload->state = (uint16_t) status->state;
				
				pj_ssize_t remaining_bytes = SSAPMSG_VARDATA_MAX_SIZE(ssapcall_status_payload_t);
				if(pj_strlen(&status->state_text) > remaining_bytes) {
					PJ_LOG(3, (THIS_FILE, "Payload overflow in %s message of %dB! Excessive bytes dropped!.", 
								ssapmsgtype_str(datagram->type),
								pj_strlen(&status->state_text) - remaining_bytes));
				}
				remaining_bytes -= pjstr_extract(datagram_payload->state_text, &status->state_text, remaining_bytes);

				datagram->payload_size = sizeof(ssapcall_status_payload_t) + 
					SSAPMSG_VARDATA_MAX_SIZE(ssapcall_status_payload_t) - remaining_bytes;
				res = PJ_SUCCESS;
				break;
			}
	  case SSAPCALL_MIC_SENSITIVITY:
		case SSAPCALL_MUTE_MIC:
	  case SSAPCALL_SPEAKER_VOLUME:
		case SSAPCALL_MUTE_SPEAKER:
	  case SSAPCALL_DTMF:
	  case SSAPCALL_QUALITY:
	  case SSAPCALL_INFO:
			PJ_LOG(3, (THIS_FILE, "Method type (%s) not implemented.", ssapmsgtype_str(datagram->type)));
			datagram->payload_size = 0;
			res = PJ_EINVAL;
			break;

		case SSAPMSG_SCAIP:
			{
				const ssapmsg_scaip_t* const msg = (const ssapmsg_scaip_t* const) payload;
				ssapmsg_scaip_payload_t* const datagram_payload = &datagram->payload.msg_scaip;

				datagram_payload->msg_offset = pj_strlen(&msg->receiver_url) +1;
				pj_ssize_t remaining_bytes = SSAPMSG_VARDATA_MAX_SIZE(ssapmsg_scaip_payload_t);

				const pj_ssize_t vardata_length = pj_strlen(&msg->receiver_url) +1 
					+ pj_strlen(&msg->msg) +1;
				if(vardata_length > remaining_bytes) {
					PJ_LOG(3, (THIS_FILE, "Payload overflow in %s message of %dB! Excessive bytes dropped!.", 
								ssapmsgtype_str(datagram->type), 
								vardata_length - remaining_bytes));
				}

				remaining_bytes -= pjstr_extract(datagram_payload->data, &msg->receiver_url, remaining_bytes);
				remaining_bytes -= pjstr_extract(datagram_payload->data + datagram_payload->msg_offset, &msg->msg, remaining_bytes);

				datagram->payload_size = sizeof(ssapmsg_scaip_payload_t) + vardata_length;
				res = PJ_SUCCESS;
				break;
			}
		case SSAPMSG_DTMF:
	  case SSAPMSG_PLAIN:
			PJ_LOG(3, (THIS_FILE, "Method type (%s) not implemented.", ssapmsgtype_str(datagram->type)));
			datagram->payload_size = 0;
			datagram->payload.raw[0] = '\0';
			res = PJ_EINVAL;
			break;

	  case SSAPCONFIG_QUIT:		// fall through
	  case SSAPCONFIG_RELOAD:
			/* quit and reload do not carry a payload. */
			datagram->payload_size = 0;
			datagram->payload.raw[0] = '\0';
			res = PJ_SUCCESS;
			break;

	  case SSAPCONFIG_STATUS:
			PJ_LOG(3, (THIS_FILE, "Method type (%s) not implemented.", ssapmsgtype_str(datagram->type)));
			datagram->payload_size = 0;
			datagram->payload.raw[0] = '\0';
			res = PJ_EINVAL;
			break;
		case SSAPCONFIG_ACCOUNT_ADD:
			{
				const ssapconfig_account_add_t* const acc_add = (const ssapconfig_account_add_t* const) payload;
				ssapconfig_account_add_payload_t* const datagram_payload = &datagram->payload.config_account_add;

				uint16_t est_length = pj_strlen(&acc_add->sip_address) + pj_strlen(&acc_add->registrar)
						+ pj_strlen(&acc_add->auth_realm) + pj_strlen(&acc_add->auth_user)
						+ pj_strlen(&acc_add->auth_pass);

				if(est_length > SSAPMSG_VARDATA_MAX_SIZE(ssapconfig_account_add_payload_t)) {
					PJ_LOG(3, (THIS_FILE, "Payload overflow in %s message of %dB! Excessive bytes dropped!.", 
								ssapmsgtype_str(datagram->type), 
								est_length - SSAPMSG_VARDATA_MAX_SIZE(ssapconfig_account_add_payload_t)));
				}

				datagram_payload->registrar_offset = 
						pjstr_extract(datagram_payload->data, &acc_add->sip_address, SSAPMSG_VARDATA_MAX_SIZE(ssapconfig_account_add_payload_t));
				datagram_payload->auth_realm_offset =
						pjstr_extract(datagram_payload->data + datagram_payload->registrar_offset,  &acc_add->registrar,  SSAPMSG_VARDATA_MAX_SIZE(ssapconfig_account_add_payload_t) - datagram_payload->registrar_offset);
				datagram_payload->auth_user_offset = 
						pjstr_extract(datagram_payload->data + datagram_payload->auth_realm_offset, &acc_add->auth_realm, SSAPMSG_VARDATA_MAX_SIZE(ssapconfig_account_add_payload_t) - datagram_payload->auth_realm_offset);
				datagram_payload->auth_pass_offset = 
						pjstr_extract(datagram_payload->data + datagram_payload->auth_user_offset,  &acc_add->auth_user, 	SSAPMSG_VARDATA_MAX_SIZE(ssapconfig_account_add_payload_t) - datagram_payload->auth_user_offset);
						pjstr_extract(datagram_payload->data + datagram_payload->auth_pass_offset,  &acc_add->auth_pass, 	SSAPMSG_VARDATA_MAX_SIZE(ssapconfig_account_add_payload_t) - datagram_payload->auth_pass_offset);

				datagram->payload_size = est_length;
				res = PJ_SUCCESS;
				break;
			}
		case SSAPCONFIG_ACCOUNT_DELETE:
			datagram->payload.config_account_del.id = (uint16_t) ((ssapconfig_account_del_t* const) payload)->id;
			datagram->payload_size = sizeof(ssapconfig_account_del_payload_t);
			break;
		case SSAPCONFIG_ACCOUNT_LIST:
			{
				const ssapconfig_account_list_t* const acc_list = (const ssapconfig_account_list_t* const) payload;
				ssapconfig_account_list_payload_t* const datagram_payload = &datagram->payload.config_account_list;

				/* Check for overflow. */
				if(acc_list->acc_no * sizeof(datagram_payload->acc[0]) > SSAPMSG_VARDATA_MAX_SIZE(ssapconfig_account_list_payload_t)) {
					PJ_LOG(3, (THIS_FILE, "Payload overflow in %s message! Account list truncated!.", 
								ssapmsgtype_str(datagram->type)));
					datagram_payload->acc_no = (uint16_t) (SSAPMSG_VARDATA_MAX_SIZE(ssapconfig_account_list_payload_t) / sizeof(datagram_payload->acc[0]));
				}
				else {
					datagram_payload->acc_no = (uint16_t) acc_list->acc_no;
				}
				
				for(int i=0; i!=datagram_payload->acc_no; ++i) {
					datagram_payload->acc[i] = (uint16_t) acc_list->acc[i];
				}

				datagram->payload_size = sizeof(ssapconfig_account_list_payload_t)
					+ datagram_payload->acc_no * sizeof(datagram_payload->acc[0]);
				res = PJ_SUCCESS;
				break;
			}
		case SSAPCONFIG_ACCOUNT_INFO:
			{
				const pjsua_acc_info* const acc_info = (const pjsua_acc_info* const) payload;
				ssapconfig_account_info_payload_t* const datagram_payload = &datagram->payload.config_account_info;

				if(pj_strlen(&acc_info->acc_uri) > SSAPMSG_VARDATA_MAX_SIZE(ssapconfig_account_info_payload_t)) {
					PJ_LOG(3, (THIS_FILE, "Payload overflow in %s message of %dB! Excessive bytes dropped!.", 
								ssapmsgtype_str(datagram->type), 
								pj_strlen(&acc_info->acc_uri) - SSAPMSG_VARDATA_MAX_SIZE(ssapconfig_account_info_payload_t)));
				}
				
				datagram_payload->id = (uint16_t) acc_info->id;
				datagram_payload->reg_code = (uint32_t) acc_info->status;
				datagram_payload->reg_expires = (uint32_t) acc_info->expires;
				datagram_payload->reg_last_error = (uint32_t) acc_info->reg_last_err;
				datagram_payload->is_default_acc = (uint16_t) acc_info->is_default;
				datagram_payload->is_online = (uint16_t) acc_info->online_status;
				uint16_t cp_bytes = pjstr_extract(datagram_payload->sip_address, &acc_info->acc_uri, SSAPMSG_VARDATA_MAX_SIZE(ssapconfig_account_info_payload_t));
	
				datagram->payload_size = cp_bytes + sizeof(ssapconfig_account_info_payload_t);
				res = PJ_SUCCESS;
				break;
			}
		case SSAPCONFIG_BUDDY_ADD:
			{
				const ssapconfig_buddy_add_t* const buddy_add = (const ssapconfig_buddy_add_t* const) payload;
				ssapconfig_buddy_add_payload_t* const datagram_payload = &datagram->payload.config_buddy_add;

				if(pj_strlen(&buddy_add->sip_url) > SSAPMSG_VARDATA_MAX_SIZE(ssapconfig_buddy_add_payload_t)) {
					PJ_LOG(3, (THIS_FILE, "Payload overflow in %s message of %dB! Excessive bytes dropped!.", 
								ssapmsgtype_str(datagram->type), 
								pj_strlen(&buddy_add->sip_url) - SSAPMSG_VARDATA_MAX_SIZE(ssapconfig_buddy_add_payload_t)));
				}

				datagram->payload_size = 
					pjstr_extract(datagram_payload->sip_url, &buddy_add->sip_url, SSAPMSG_VARDATA_MAX_SIZE(ssapconfig_buddy_add_payload_t));
				res = PJ_SUCCESS;
				break;
			}
		case SSAPCONFIG_BUDDY_DELETE:
			datagram->payload.config_buddy_del.id = (uint16_t) ((ssapconfig_buddy_del_t* const) payload)->id;
			datagram->payload_size = sizeof(ssapconfig_buddy_del_payload_t);
			res = PJ_SUCCESS;
			break;
		case SSAPCONFIG_BUDDY_LIST:
			{
				const ssapconfig_buddy_list_t* const buddy_list = (const ssapconfig_buddy_list_t* const) payload;
				ssapconfig_buddy_list_payload_t* const datagram_payload = &datagram->payload.config_buddy_list;

				/* Check for overflow. */
				if(buddy_list->buddy_no * sizeof(datagram_payload->buddy[0]) > SSAPMSG_VARDATA_MAX_SIZE(ssapconfig_buddy_list_payload_t)) {
					PJ_LOG(3, (THIS_FILE, "Payload overflow in %s message! Account list truncated!.", 
								ssapmsgtype_str(datagram->type)));
					datagram_payload->buddy_no = (uint16_t) (SSAPMSG_VARDATA_MAX_SIZE(ssapconfig_buddy_list_payload_t) / sizeof(datagram_payload->buddy[0]));
				}
				else {
					datagram_payload->buddy_no = (uint16_t) buddy_list->buddy_no;
				}
				
				for(int i=0; i!=datagram_payload->buddy_no; ++i) {
					datagram_payload->buddy[i] = (uint16_t) buddy_list->buddy[i];
				}

				datagram->payload_size = sizeof(ssapconfig_buddy_list_payload_t)
					+ datagram_payload->buddy_no * sizeof(datagram_payload->buddy[0]);
				res = PJ_SUCCESS;
				break;
			}
		case SSAPCONFIG_BUDDY_INFO:
			{
				const pjsua_buddy_info* const buddy_info = (const pjsua_buddy_info* const) payload;
				ssapconfig_buddy_info_payload_t* const datagram_payload = &datagram->payload.config_buddy_info;

				if(pj_strlen(&buddy_info->uri) > SSAPMSG_VARDATA_MAX_SIZE(ssapconfig_buddy_info_payload_t)) {
					PJ_LOG(3, (THIS_FILE, "Payload overflow in %s message of %dB! Excessive bytes dropped!.", 
								ssapmsgtype_str(datagram->type), 
								pj_strlen(&buddy_info->uri) - SSAPMSG_VARDATA_MAX_SIZE(ssapconfig_buddy_info_payload_t)));
				}

				datagram_payload->id = (uint16_t) buddy_info->id;
				pj_ssize_t cp_bytes = pjstr_extract(datagram_payload->sip_url, &buddy_info->uri, SSAPMSG_VARDATA_MAX_SIZE(ssapconfig_buddy_info_payload_t));
				datagram->payload_size = sizeof(ssapconfig_buddy_info_payload_t) + cp_bytes;
				res = PJ_SUCCESS;
				break;
			}

	  case SSAPPJSUA:
			{
				const ssappjsua_pjsua_t* const msg = (const ssappjsua_pjsua_t* const) payload;
				ssappjsua_pjsua_payload_t* const p = &datagram->payload.pjsua;

				if(pj_strlen(&msg->pjsua_cmd) > SSAPMSG_VARDATA_MAX_SIZE(ssappjsua_pjsua_payload_t)) {
					PJ_LOG(3, (THIS_FILE, "Payload overflow in %s message of %dB! Excessive bytes dropped!.", 
								ssapmsgtype_str(datagram->type), 
								pj_strlen(&msg->pjsua_cmd) - SSAPMSG_VARDATA_MAX_SIZE(ssappjsua_pjsua_payload_t)));
				}

				datagram->payload_size = 
					pjstr_extract(p->pjsua_cmd, &msg->pjsua_cmd, SSAPMSG_VARDATA_MAX_SIZE(ssappjsua_pjsua_payload_t));
				res = PJ_SUCCESS;
				break;
			}

		default:
			PJ_LOG(3, (THIS_FILE, "Method type (%s) not implemented.", ssapmsgtype_str(datagram->type)));

			datagram->type = SSAPMSG_UNDEFINED;
			datagram->payload_size = 0;
			res = PJ_EINVAL;
			break;
	}
				
	// crc is calculated at comms level.
	
	if(datagram->payload_size == 0) {
		datagram->payload.raw[0] = '\0';
	}

	return res;
}

pj_ssize_t ssapmsg_send(ssapmsg_datagram_t* const msg) {
  /* Add crc to packet. */
  update_crc32(msg);

  pj_ssize_t msg_size = ssapmsg_datagram_size(msg);

	PJ_LOG(3, (THIS_FILE, "Sending data:"));
	print_raw_hex(msg, msg_size);

  const pj_ssize_t expected_transmission_length = msg_size;
  pj_status_t res = ssapsock_send((void*) msg, &msg_size);

	//TODO: Add error handling and retransmission.
  if( res == PJ_SUCCESS ) {
    if( msg_size != expected_transmission_length ) {
      PJ_LOG(3, (THIS_FILE, "Failed sending expected number of bytes. Expected: %d, sent: %d", expected_transmission_length, msg_size));
      res = PJ_EBUG;
    }

#if 0
    /* Message transmission successful. Clear pending response. */
    response_token.ref = 0u;
    response_token.type = SSAPMSG_UNDEFINED;
#endif
  }

  return res;
}

pj_status_t ssapmsg_send_request(const ssapmsg_t type, const void* const payload) {
	PJ_PERROR(1, (THIS_FILE, PJ_EINVALIDOP, "Request transmissions not implemented."));
	return PJ_EINVALIDOP;
}


pj_status_t ssapmsg_send_reply(const ssapmsg_t type, const void* const payload) {
	ssapmsg_datagram_t datagram = {
		.ref = response_token.ref,
		.protocol_version = SSAPMSG_PROTOCOL_VERSION,
		.type = type,
		.payload_size = 0,
		.crc = 0,
		.payload.raw[0] = '\0'
	};

  if(response_token.type == SSAPMSG_UNDEFINED) {
    PJ_LOG(3, (THIS_FILE, "Message flow is out of sync! No response is pending."));
    datagram.ref = SSAPMSG_MESSAGE_FLOW_OUT_OF_SYNC;
  }
	else {
		/* clearing pending reply. */
		response_token.ref = SSAPMSG_NO_RESPONSE_PENDING;
		response_token.type = SSAPMSG_UNDEFINED;
	}

	pj_status_t res = ssapmsg_pack(&datagram, payload);
	if(res == PJ_SUCCESS) {
		const pj_ssize_t bytes_sent = ssapmsg_send(&datagram);
  	res = (bytes_sent == ssapmsg_datagram_size(&datagram))
  	  ? PJ_SUCCESS
  	  : -1;
	}
	else {
		PJ_PERROR(3, (THIS_FILE, res, "Could not pack message. Will not send."));
	}

	return res;
}

pj_status_t ssapmsg_send_status(const ssapmsg_t type, const void* const payload) {
	ssapmsg_datagram_t datagram;
	datagram.ref = SSAPMSG_STATUS_REFERENCE;
	datagram.protocol_version = SSAPMSG_PROTOCOL_VERSION;
	datagram.type = type;
	// crc is calculated at comms level.
	
	pj_status_t res = ssapmsg_pack(&datagram, payload);
	if(res == PJ_SUCCESS) {
		const pj_ssize_t bytes_sent = ssapmsg_send(&datagram);
  	res = (bytes_sent == ssapmsg_datagram_size(&datagram))
  	  ? PJ_SUCCESS
  	  : -1;
	}
	else {
		PJ_PERROR(3, (THIS_FILE, res, "Could not pack message. Will not send."));
	}

	return res;
}

pj_status_t ssapmsg_response(const pj_status_t status, const char* const msg) {
	char msg_buf[SSAPMSG_VARDATA_MAX_SIZE(ssapmsg_status_t)];
	ssapmsg_status_t s = {
		.code = status
	};
	pj_cstr(&s.msg, msg);

	ssapmsg_t t;

	if(status == PJ_SUCCESS) {
		t = SSAPMSG_ACK;
		s.msg = pj_str(NULL);
	}
	else {
		t = SSAPMSG_NACK;
		/* If no error message is provided, lookup standard message for response type. */
		s.msg = (msg != NULL) 
			? pj_str((char*) msg)
			: pj_strerror(status, msg_buf, SSAPMSG_VARDATA_MAX_SIZE(ssapmsg_status_t));
	}

	return ssapmsg_send_reply(t, &s);
}



static pj_status_t ssapmsg_fetch_data(ssapmsg_datagram_t* msg, pj_ssize_t* lim) {
#define RCV_QUEUE_LEN (sizeof(ssapmsg_datagram_t) *2)

	/* Fetch */
	if(rcv_buffer != NULL) {
		/* Data available in queue. */
		pj_ssize_t data_len = rcv_end - rcv_buffer;
		if(data_len > SSAPMSG_HEADER_SIZE
				&& ssapmsg_datagram_size((ssapmsg_datagram_t*)rcv_buffer) >= data_len) {
			/* Message is ready in buffer. */
			*lim = ssapmsg_datagram_size((ssapmsg_datagram_t*)rcv_buffer);
			pj_memcpy(msg, rcv_buffer, *lim);

			/* If buffer is empty, relase it. */
			if(data_len == *lim) {
				pj_pool_release(rcv_pool);
				rcv_buffer = NULL;
				rcv_end = NULL;
				rcv_pool = NULL;
			}
			else {
				/* Move data for next operation. */
				pj_ssize_t overflow = data_len - *lim;
				pj_memmove(rcv_buffer, rcv_buffer + *lim, overflow);
				rcv_end = rcv_buffer + overflow;
			}

			return PJ_SUCCESS;
		}
		/* else: Need to fetch more data. */
	}

 	pj_status_t res = ssapsock_receive((uint8_t*) msg, lim);

	if(res == PJ_SUCCESS) {
		if(*lim == 0) {
			return res;
		}

		if(*lim < SSAPMSG_HEADER_SIZE 
				|| ssapmsg_datagram_size(msg) != *lim) {
			/* Received either too much or too little data. */

			if(rcv_pool == NULL) {
  		  PJ_LOG(2, (THIS_FILE, "Allocating memory for receive queue."));
  		  rcv_pool = pj_pool_create(&ssap_mem.factory, "receive_queue", RCV_QUEUE_LEN, 0, NULL);
  		  if(rcv_pool == NULL) {
  		    PJ_PERROR(3, (THIS_FILE, PJ_ENOMEM, "Could not allocate memory for receive queue"));
  		    return PJ_ENOMEM;
  		  }

				rcv_buffer = pj_pool_alloc(rcv_pool, sizeof(ssapmsg_datagram_t) *2);
				if(rcv_buffer == NULL) {
  			  PJ_PERROR(3, (THIS_FILE, PJ_ENOMEM, "Could not allocate memory for rcv fragment."));
  			  return PJ_ENOMEM;
  			}

				rcv_end = rcv_buffer;
			}

			const pj_ssize_t overflow = (*lim >= SSAPMSG_HEADER_SIZE && *lim > ssapmsg_datagram_size(msg))
				? *lim - ssapmsg_datagram_size(msg)
				: *lim;
			*lim -= overflow;

			if(rcv_end + overflow > rcv_buffer + RCV_QUEUE_LEN) {
				/* Memory queue corrupted. Not enough space. Drop it. */
				PJ_PERROR(3, (THIS_FILE, SSAP_EMEMCORRUPT, "Message queue corrupted. Dropping queue."));
				pj_pool_release(rcv_pool);
				rcv_buffer = NULL;
				rcv_end = NULL;
				rcv_pool = NULL;

				return SSAP_EMEMCORRUPT;
			}

			pj_memcpy(rcv_end, (uint8_t*)msg + *lim, overflow);
			rcv_end += overflow;

			return *lim > 0
				? PJ_SUCCESS
				: PJ_ETOOSMALL;
		}
		/* else: received data matches a message. */
	}

	return res;
}

static pj_bool_t valid_datagram(ssapmsg_datagram_t* const d) {
	if(!reference_is_valid(d->ref)) {
		PJ_LOG(3, (THIS_FILE, "Reject datagram due to invalid reference."));
		return PJ_FALSE;
	}

	if(d->protocol_version != SSAPMSG_PROTOCOL_VERSION) {
		PJ_LOG(3, (THIS_FILE, "Reject datagram due to invalid protocol version."));
		return PJ_FALSE;
	}

	char* p = ssapmsgtype_str(d->type);
	if(p == "" || p == NULL) {
		PJ_LOG(3, (THIS_FILE, "Reject datagram due to invalid message type."));
		return PJ_FALSE;
	}
	
	if(d->payload_size > SSAPMSG_MAX_PAYLOAD_SIZE) {
		PJ_LOG(3, (THIS_FILE, "Reject datagram due to invalid size."));
		return PJ_FALSE;
	}

	if(validate_ssapmsg_crc(d) != PJ_SUCCESS) {
		PJ_LOG(3, (THIS_FILE, "Reject datagram due to invalid crc."));
		return PJ_FALSE;
	}

	return PJ_TRUE;
}

