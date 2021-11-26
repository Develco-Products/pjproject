#include <pjsua-lib/pjsua.h>
#include "pjsua_app_common.h"

#include "pjsua_ssapmsg.h"
#include "pjlib.h"
#include "pjsua_app.h"
#include <string.h>
#include "pj/types.h"
#include "pjlib-util/crc32.h"
#include "pjsua_ssapcomm.h"

#define THIS_FILE "pjsua_ssapmsg"

/* keeps track of last sent message. 
 * Keep this static to initialize to 0. */
#if 0
static struct {
  uint16_t ref;
	uint16_t protocol_version;
  ssapmsg_t type;
} response_token;
#endif

static ssapmsg_datagram_t response_token;

#if 0
static union ssapmsg_payload buffer;
static union ssapmsg_payload* shared_buffer = NULL;
#endif

static void sync_response(ssapmsg_datagram_t* const datagram);

/* Make a safe conversion of pj_str to char* buffer. Including handling hidden
 * null-strings and strings with missing null-terminators. */
void pjstr_extract(char* const dst, const pj_str_t* const pj_str, const ssize_t max_length) {
	const char* const s = pj_strbuf(pj_str);
	const pj_size_t pj_str_length = pj_strlen(pj_str);
	if(s == NULL || pj_str_length == 0) {
		dst[0] = '\0';
	}
	else {
		const pj_ssize_t cp_bytes = (pj_str_length > max_length -1) 
			? max_length -1 
			: pj_str_length;
		pj_memcpy(dst, s, cp_bytes);
		dst[cp_bytes] = '\0';
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

	  case SSAPPJSUA:
	  	ret = "SSAPPJSUA";
      break;

    default:
      ret = "INVALID SSAPMSG enum value.";
      break;
  }

  return ret;
}

#if 0
pj_status_t ssapmsg_parse(struct ssapmsg_iface* dst, ssapmsg_datagram_t* datagram) {
  pj_status_t res = PJ_SUCCESS;

  dst->ref  = datagram->ref;
  dst->type = datagram->type;

  /* Add any special handling of specific payload types. */
  switch(datagram->type) {
	  case SSAPMSG_UNDEFINED:
	  case SSAPMSG_ACK:
	  case SSAPMSG_NACK:
	  case SSAPMSG_WARNING:
	  case SSAPMSG_ERROR:
      PJ_LOG(3, (THIS_FILE, "Parsing not implemented for: %s", ssapmsgtype_str(datagram->type)));
      res = PJ_EINVAL;
      break;

	  case SSAPCALL_DIAL:


	  case SSAPCALL_HANGUP:
	  case SSAPCALL_MIC_SENSITIVITY:
	  case SSAPCALL_SPEAKER_VOLUME:
	  case SSAPCALL_DTMF:
	  case SSAPCALL_QUALITY:
	  case SSAPCALL_INFO:
      PJ_LOG(3, (THIS_FILE, "Parsing not implemented for: %s", ssapmsgtype_str(datagram->type)));
      res = PJ_EINVAL;
      break;

	  case SSAPMSG_SCAIP:
      /* Initialize address and message pointers to correct points in the data buffer. */
      if(datagram->payload.msg_scaip.msg_offset >= datagram->payload_size) {
        PJ_LOG(3, (THIS_FILE, "Contents of scaip message corrupted."));
        res = PJ_EINVAL;
      }
      else {
        dst->address = datagram->payload.msg_scaip.data;
        dst->msg = datagram->payload.msg_scaip.data + datagram->payload.msg_scaip.msg_offset;
        res = PJ_SUCCESS;
      }
      break;
	  case SSAPMSG_PLAIN:

	  case SSAPCONFIG_QUIT:
	  case SSAPCONFIG_RELOAD:
	  case SSAPCONFIG_STATUS:
      PJ_LOG(3, (THIS_FILE, "Parsing not implemented for: %s", ssapmsgtype_str(datagram->type)));
      res = PJ_EINVAL;
      break;

	  case SSAPPJSUA:
      // pjsua commands are textbased, so ensure null-termination.
      dst->data = datagram->payload.pjsua.data;
      //dst->raw[hdr->payload_size] = '\0';
      res = PJ_SUCCESS;
      break;

    defualt:
      PJ_LOG(3, (THIS_FILE, "Invalid payload type (%d) in header.", datagram->type));
      res = PJ_EINVAL;
      break;
  }

  return res;
}
#endif

void ssapmsg_print(ssapmsg_datagram_t* const msg) {
  char buffer[1024];

#if 0
  PJ_LOG(3, (THIS_FILE, "Info ---------------------"));
  PJ_LOG(3, (THIS_FILE, "  sizeof datagrame	:  %d / 0x%X", sizeof(ssapmsg_datagram_t), sizeof(ssapmsg_datagram_t)));
  PJ_LOG(3, (THIS_FILE, "  sizeof payload   :  %d / 0x%X", sizeof(union ssapmsg_payload), sizeof(union ssapmsg_payload)));
  PJ_LOG(3, (THIS_FILE, "  sizeof header    :  %d / 0x%X", SSAPMSG_HEADER_SIZE, SSAPMSG_HEADER_SIZE));
  PJ_LOG(3, (THIS_FILE, "--------------------------"));

	ssapmsg_datagram_t d;
	d.ref = 0x1122;
	d.protocol_version = 0x3344;
	d.type = SSAPCONFIG_STATUS;
	d.payload_size = 0x5566;
	d.crc = 0x778899AA;
	d.payload.msg_scaip.msg_offset = 0xBBCC;
	print_raw_hex((uint8_t*) &d, 64);
#endif

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
	  case SSAPCONFIG_RELOAD:
	  case SSAPCONFIG_STATUS:
      PJ_LOG(3, (THIS_FILE, "  - not implemented -"));
      break;

	  case SSAPPJSUA:
      PJ_LOG(3, (THIS_FILE, "  data:         %s", msg->payload.pjsua.data));
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

void ssapmsg_clone(ssapmsg_datagram_t* const dst, const ssapmsg_datagram_t* const src) {
	const pj_ssize_t len = src->payload_size + SSAPMSG_HEADER_SIZE;
	pj_memcpy((void*) dst, src, len);
}

pj_bool_t ssapmsg_is_status(const ssapmsg_datagram_t* const d) {
	return d->ref == SSAPMSG_STATUS_REFERENCE;
}

pj_ssize_t ssapmsg_datagram_size(const ssapmsg_datagram_t* const msg) {
	return SSAPMSG_HEADER_SIZE + msg->payload_size;
}

const char* const ssapmsg_scaip_address(const ssapmsg_datagram_t* msg) {
  return msg->payload.msg_scaip.data;
}


const char* const ssapmsg_scaip_message(const ssapmsg_datagram_t* msg) {
  return msg->payload.msg_scaip.data + msg->payload.msg_scaip.msg_offset;
}


pj_status_t ssapmsg_receive(ssapmsg_datagram_t* const msg) {
  pj_ssize_t lim = sizeof(ssapmsg_datagram_t);
  pj_status_t res = ssapsock_receive((uint8_t*) msg, &lim);

  switch(res) {
    case PJ_SUCCESS:
      /* Validate that packet has valid size value. */
      if(msg->payload_size + SSAPMSG_HEADER_SIZE != lim) {
        PJ_LOG(3, (THIS_FILE, "Packet is malformed. Size: %dB, Received: %d", msg->payload_size + SSAPMSG_HEADER_SIZE, lim));
        return PJ_EINVAL;
      }

      /* validate packet crc. */
      if(validate_ssapmsg_crc(msg) != PJ_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "crc validation failed. Currently results in no action."));
      }

      /* Set token for response. */
      //struct response_token* const token = &response_token[msg->type];

      if(response_token.type != SSAPMSG_UNDEFINED) {
        PJ_LOG(3, (THIS_FILE, "Overwriting existing pending response token for: %s with id: %d", ssapmsgtype_str(response_token.type), response_token.ref));
      }

#if 1
			if(msg->ref < INT16_MAX) {
				//TODO: Add async message handling.
      	response_token.ref  = msg->ref;
      	response_token.protocol_version = msg->protocol_version;
      	response_token.type = msg->type;
			}
			/* else: status message. Do not track reference for a reply. */
#endif
			//ssapmsg_clone(response_token, msg);

      PJ_LOG(3, (THIS_FILE, "Setting pending response for %s with id: %d", ssapmsgtype_str(response_token.type), response_token.ref));
      break;

    case PJ_STATUS_FROM_OS(ENOTSOCK):
      {
        const char* const cmd_quit = "q\n";
        const uint32_t l = strlen(cmd_quit) +1;
        //ssapmsg_datagram_t msg = {
        //  .ref = 0,
        //  .type = SSAPMSG_PJSUA,
        //  .payload_size = l,
        //  .crc = 0
        //};
        //pj_memcpy((void*) &msg.payload.pjsua, cmd_quit, l);

        msg->ref = 0; // ref dummy value.
				msg->protocol_version = 0; // dummy version
        msg->type = SSAPPJSUA;
        msg->payload_size = l;
        msg->crc = 0u;  // dummy crc.
        pj_memcpy(msg->payload.pjsua.data, cmd_quit, l);

        //lim = SSAPMSG_HEADER_SIZE + l;
        //pj_memcpy(inp, &msg, *lim);
				//

        //PJ_PERROR(1, (THIS_FILE, res, "  %s ", __func__));
        /* Signal for program to quit. */
        //strcpy(inp, "q\n");
        res = PJ_SUCCESS;
      }
      break;

    default:
      /* No a result which is handled here. */
      break;
  }

  return res;
}

pj_ssize_t ssapmsg_send(ssapmsg_datagram_t* const msg) {
  /* Add crc to packet. */
  update_crc32(msg);

  pj_ssize_t msg_size = msg->payload_size + SSAPMSG_HEADER_SIZE;

	PJ_LOG(3, (THIS_FILE, "Sending data:"));
	print_raw_hex(msg, msg_size);

  const pj_ssize_t expected_transmission_length = msg_size;
  pj_status_t res = ssapsock_send((void*) msg, &msg_size);

  /* Error handling already exists at socket layer.
   * Check for size mismatch for succussful transmissions. */
  if( res == PJ_SUCCESS ) {
    if( msg_size != expected_transmission_length ) {
      PJ_LOG(3, (THIS_FILE, "Failed sending expected number of bytes. Expected: %d, sent: %d", expected_transmission_length, msg_size));
      res = PJ_EBUG;
    }

    /* Message transmission successful. Clear pending response. */
    response_token.ref = 0u;
    response_token.type = SSAPMSG_UNDEFINED;
  }

  return res;
}

pj_status_t ssapmsg_response_send(const pj_status_t status, const char* const msg) {
  ssapmsg_datagram_t datagram;
	sync_response(&datagram);

	const ssize_t msg_length = (msg != NULL)
		? strlen(msg) +1
		: 1;

	datagram.type = (status == PJ_SUCCESS)
		? SSAPMSG_ACK
		: SSAPMSG_NACK;
	datagram.payload_size = sizeof(ssapmsg_status_t) + msg_length;

	/* Ack and Nack share the status_t payload type. */
	ssapmsg_status_t* payload = (ssapmsg_status_t*) &datagram.payload;

	payload->code = status;
	if(msg != NULL) {
		pj_memcpy(payload->msg, msg, msg_length);
	}
	else {
		payload->msg[0] = '\0';
	}

	const pj_ssize_t bytes_sent = ssapmsg_send(&datagram);
	
  return (bytes_sent == ssapmsg_datagram_size(&datagram))
    ? PJ_SUCCESS
    : -1;
}

pj_status_t ssapcall_dial_send(const char* const addr) {
  ssapmsg_datagram_t datagram;
  const ssize_t addr_length = strlen(addr) +1;

  sync_response(&datagram);
  datagram.type = SSAPCALL_DIAL;
  datagram.payload_size = sizeof(ssapmsg_scaip_t) + addr_length;

	/* Pack Payload */
	ssapcall_dial_t* p = &datagram.payload.call_dial;
	pj_memcpy(p->sip_address, addr, addr_length);

  /* Send it. */
  return ssapmsg_send(&datagram) == ssapmsg_datagram_size(&datagram)
		? PJ_SUCCESS
		: -1;
}

pj_status_t ssapcall_status_send(const pjsua_call_info* const call_state) {
	ssapmsg_datagram_t datagram;
	const pj_ssize_t state_len = pj_strlen(&call_state->state_text) +1;

	sync_response(&datagram);
	datagram.type = SSAPCALL_STATUS;
	datagram.payload_size = sizeof(ssapcall_status_t) + state_len;

	/* Pack payload */
	ssapcall_status_t* p = &datagram.payload.call_status;
	p->id = call_state->id;
	p->state = call_state->state;

	pjstr_extract(p->state_text, &call_state->state_text, SSAPMSG_VARDATA_SIZE(ssapcall_status_t));

  /* Send it. */
  return ssapmsg_send(&datagram) == ssapmsg_datagram_size(&datagram)
		? PJ_SUCCESS
		: -1;
}

pj_status_t ssapmsg_scaip_send(const char* const addr, const char* const msg) {
  ssapmsg_datagram_t datagram;
  const ssize_t addr_length = strlen(addr) +1;
  const ssize_t msg_length = strlen(msg) +1;

  sync_response(&datagram);
  datagram.type = SSAPMSG_SCAIP;
  datagram.payload_size = sizeof(ssapmsg_scaip_t) + addr_length + msg_length;

  /* Pack payload. */
  ssapmsg_scaip_t* p = &datagram.payload.msg_scaip;
  pj_memcpy(p->data, addr, addr_length);
  p->msg_offset = addr_length +1;
  pj_memcpy(p->data + p->msg_offset, msg, msg_length);

  /* Send it. */
  return ssapmsg_send(&datagram) == ssapmsg_datagram_size(&datagram)
		? PJ_SUCCESS
		: -1;
}

static void sync_response(ssapmsg_datagram_t* const datagram) {
  if(response_token.type == SSAPMSG_UNDEFINED) {
    PJ_LOG(3, (THIS_FILE, "Message flow is out of sync! No response is pending."));
    datagram->ref = SSAPMSG_MESSAGE_FLOW_OUT_OF_SYNC;
  }
  else {
    datagram->ref = response_token.ref;
  }
	datagram->protocol_version = SSAPMSG_PROTOCOL_VERSION;
}

