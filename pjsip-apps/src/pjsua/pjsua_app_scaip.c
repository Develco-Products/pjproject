#include "pjsua_app_scaip.h"

#include <string.h>

#include "pjsua_app_common.h"
#include "pjsua_ssapcomm.h"
#include "pjlib.h"
#include "pjsua_app.h"
#include "pj/types.h"
//#include "pjsua_ssapmsg.h"

#define THIS_FILE "pjsua_app_scaip"

static pj_status_t make_scaip_call_str(const char* const call_data);
static pj_status_t make_scaip_call(const ssapmsg_datagram_t* const call_data);
static pj_status_t send_scaip_im_str(const char* const scaip_msg);
static pj_status_t send_scaip_im(const ssapmsg_datagram_t* const scaip_msg);
static pj_status_t send_call_status(const ssapmsg_datagram_t* const msg);

static pj_status_t make_scaip_call_str(const char* const call_data) {
  pj_status_t res;
  pj_str_t sip_address;

  pj_cstr(&sip_address, call_data);
  pj_strtrim(&sip_address);

  PJ_LOG(3, (THIS_FILE, "Verifying sip address: %s", pj_strbuf(&sip_address)));
  if((res = pjsua_verify_url(pj_strbuf(&sip_address))) != PJ_SUCCESS ) {
    PJ_PERROR(3, (THIS_FILE, res, "Invalid sip address"));
    ssapmsg_response_send(PJ_EINVAL, "Invalid sip address.");
    return res;
  }

	res = pjsua_call_make_call(current_acc, &sip_address, &call_opt, NULL,
	    NULL, &current_call);

  /* let them know how it went. */
  ssapmsg_response_send(res, NULL);

  return res;
}

static pj_status_t make_scaip_call(const ssapmsg_datagram_t* const call_data) {
  pj_status_t res;
  pj_str_t sip_address;

  pj_cstr(&sip_address, call_data->payload.call_dial.sip_address);
  pj_strtrim(&sip_address);

  PJ_LOG(3, (THIS_FILE, "Verifying sip address: %s", pj_strbuf(&sip_address)));
  if((res = pjsua_verify_url(pj_strbuf(&sip_address))) != PJ_SUCCESS ) {
    PJ_PERROR(3, (THIS_FILE, res, "Invalid sip address"));
    ssapmsg_response_send(PJ_EINVAL, "Invalid sip address.");
    return res;
  }

	res = pjsua_call_make_call(current_acc, &sip_address, &call_opt, NULL,
	    NULL, &current_call);

  /* let them know how it went. */
  ssapmsg_response_send(res, NULL);

  return res;
}

static pj_status_t send_scaip_im_str(const char* const scaip_msg) {
  pj_status_t res;
  pj_str_t mime = pj_str("application/scaip+xml");

  pj_str_t sip_address;
  pj_str_t sip_msg;

  pj_str_t msg;
  pj_cstr(&msg, scaip_msg);
  pj_str_t delim = pj_str(" ");
  pj_ssize_t e = pj_strlen(&msg);
  pj_ssize_t idx = 0;

  idx = pj_strtok(&msg, &delim, &sip_address, idx);
  idx += pj_strlen(&sip_address);
  idx = pj_strtok(&msg, &delim, &sip_msg, idx);

  char sip_address_buffer[128];
  pj_memcpy( sip_address_buffer, pj_strbuf(&sip_address), pj_strlen(&sip_address) );
  sip_address_buffer[pj_strlen(&sip_address)] = '\0';
  
  PJ_LOG(5, (THIS_FILE, "Verifying sip address: %s", sip_address));
  if((res = pjsua_verify_url(sip_address_buffer)) != PJ_SUCCESS ) {
    PJ_PERROR(3, (THIS_FILE, res, "Invalid sip address"));
    return res;
  }
  pj_strset2( &sip_address, sip_address_buffer );

  PJ_LOG(5, (THIS_FILE, "sip address: %s", sip_address));
  PJ_LOG(5, (THIS_FILE, "sip message: %s", sip_msg));

	res = pjsua_im_send(current_acc, &sip_address, &mime, &sip_msg, NULL, NULL);

  return res;
}

static pj_status_t send_scaip_im(const ssapmsg_datagram_t* const scaip_msg) {
  pj_str_t mime = pj_str("application/scaip+xml");
  pj_str_t sip_address;
  pj_str_t sip_msg;
  pj_status_t res;
  const char* const address = ssapmsg_scaip_address(scaip_msg);
  const char* const message = ssapmsg_scaip_message(scaip_msg);

  PJ_LOG(5, (THIS_FILE, "Verifying sip address: %s", address));
  if((res = pjsua_verify_url(address)) != PJ_SUCCESS ) {
    PJ_PERROR(3, (THIS_FILE, res, "Invalid sip address"));
    return res;
  }
  //pj_strset2(&sip_address, address);
  //pj_strset2(&sip_msg, message);
  pj_cstr(&sip_address, address);
  pj_cstr(&sip_msg, message);

  PJ_LOG(5, (THIS_FILE, "Sending SCAIP IM"));
  PJ_LOG(5, (THIS_FILE, "  sip address: %s", sip_address));
  PJ_LOG(5, (THIS_FILE, "  sip message: %s", sip_msg));

	res = pjsua_im_send(current_acc, &sip_address, &mime, &sip_msg, NULL, NULL);

  /* let them know how it went. */
  //ssapmsg_response_send(res, NULL);

  return res;
}

static pj_status_t send_call_status(const ssapmsg_datagram_t* const msg) {
  pjsua_call_info cinfo;
  pjsua_call_get_info(msg->payload.call_status.id, &cinfo);
  return ssapcall_status_send(&cinfo);
}

void ui_handle_scaip_cmd(const char* const inp) {
	switch(inp[0]) {
		case 'i':
		  PJ_LOG(2, (THIS_FILE, "Sending IM message: %s", inp +1));
		  send_scaip_im_str(inp +1);
		  break;

		case 'm':
		  PJ_LOG(2, (THIS_FILE, "Calling: %s", inp +1));
		  make_scaip_call_str(inp +1);
		  break;

		case '\0':
		  /*Empty command. Just ignore.*/
		  break;

		default:
		  PJ_LOG(5, (THIS_FILE, "Unknown scaip command: %c", inp[0]));
		  break;
  }
}

#if ENABLE_PJSUA_SSAP
pj_status_t ui_scaip_handler(const ssapmsg_datagram_t* const msg, char** app_cmd) {
  pj_status_t res = -1;
  PJ_LOG(3, (THIS_FILE, "Scaip command is %d", msg->type));

  switch( msg->type ) {
	  case SSAPMSG_UNDEFINED:
      PJ_LOG(3, (THIS_FILE, "Scaip command is undefined. Did you mean to send this?"));
      res = PJ_EINVAL;
      break;
	  case SSAPMSG_ACK:
	  case SSAPMSG_NACK:
	  case SSAPMSG_WARNING:
	  case SSAPMSG_ERROR:
      PJ_LOG(3, (THIS_FILE, "functionlity NOT IMPLEMENTED for %s", ssapmsgtype_str(msg->type)));
      break;

	  case SSAPCALL_DIAL:
      PJ_LOG(2, (THIS_FILE, "Dialling a new SIP call to: %s", ssapmsg_scaip_address(msg)));
      res = make_scaip_call(msg);
      break;

	  case SSAPCALL_HANGUP:
      PJ_LOG(3, (THIS_FILE, "functionlity NOT IMPLEMENTED for %s", ssapmsgtype_str(msg->type)));
      break;

		case SSAPCALL_STATUS:
      if(ssapmsg_is_status(msg)) {
        PJ_LOG(3, (THIS_FILE, "Call status has no meaning as a status message. Dropping message."));
        res = PJ_SUCCESS;
      }
      else {
        PJ_LOG(3, (THIS_FILE, "Getting call status for call id: %d", msg->payload.call_status.id));
        if(msg->payload.call_status.id < pjsua_call_get_max_count()) {
          pjsua_call_info cinfo;
          res = pjsua_call_get_info(msg->payload.call_status.id, &cinfo);
          if(res == PJ_SUCCESS) {
            res = ssapcall_status_send(&cinfo);
          }
          else {
            res = ssapmsg_response_send(res, "Failed to obtain call info.");
          }
        }
        else {
          res = ssapmsg_response_send(PJ_EINVAL, "Call id is invalid.");
        }
      }
      break;

	  case SSAPCALL_MIC_SENSITIVITY:
		case SSAPCALL_MUTE_MIC:
	  case SSAPCALL_SPEAKER_VOLUME:
		case SSAPCALL_MUTE_SPEAKER:
	  case SSAPCALL_DTMF:
	  case SSAPCALL_QUALITY:
	  case SSAPCALL_INFO:
      PJ_LOG(3, (THIS_FILE, "functionlity NOT IMPLEMENTED for %s", ssapmsgtype_str(msg->type)));
      break;

	  case SSAPMSG_SCAIP:
      PJ_LOG(2, (THIS_FILE, "Sending IM message: %s", ssapmsg_scaip_address(msg)));
      res = send_scaip_im(msg);
      break;
		case SSAPMSG_DTMF:
	  case SSAPMSG_PLAIN:

	  case SSAPCONFIG_QUIT:
	  case SSAPCONFIG_RELOAD:
	  case SSAPCONFIG_STATUS:

	  case SSAPPJSUA:
      PJ_LOG(3, (THIS_FILE, "functionlity NOT IMPLEMENTED for %s", ssapmsgtype_str(msg->type)));
      break;

    default:
      PJ_LOG(3, (THIS_FILE, "Unknown scaip command: %d", msg->type));
      res = PJ_EINVAL;
      break;
  }

  return res;
}
#endif

