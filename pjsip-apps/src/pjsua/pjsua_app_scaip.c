#include "pjsua_app_scaip.h"

#include "pjsua_app_common.h"

#include "pjsua_ssapcomm.h"
#include "pjlib.h"
#include "pjsua_app.h"
#include <string.h>
#include "pj/types.h"
//#include "pjsua_ssapmsg.h"

#define THIS_FILE "pjsua_app_scaip"



static pj_status_t make_scaip_call(const char* const call_data) {
  pj_status_t res;
  pj_str_t sip_address;

  pj_cstr(&sip_address, call_data);
  pj_strtrim(&sip_address);

  PJ_LOG(3, (THIS_FILE, "Verifying sip address: %s", pj_strbuf(&sip_address)));
  if((res = pjsua_verify_url(pj_strbuf(&sip_address))) != PJ_SUCCESS ) {
    PJ_PERROR(3, (THIS_FILE, res, "Invalid sip address"));
    return res;
  }

	res = pjsua_call_make_call(current_acc, &sip_address, &call_opt, NULL,
	    NULL, &current_call);

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

  /* let them know how it went. */
  //ssapmsg_response_send(res, NULL);

  return res;
}

static pj_status_t send_scaip_im(const struct ssapmsg_iface* const scaip_msg) {
  pj_str_t mime = pj_str("application/scaip+xml");
  pj_str_t sip_address;
  pj_str_t sip_msg;
  pj_status_t res;

  PJ_LOG(5, (THIS_FILE, "Verifying sip address: %s", scaip_msg->address));
  if((res = pjsua_verify_url(scaip_msg->address)) != PJ_SUCCESS ) {
    PJ_PERROR(3, (THIS_FILE, res, "Invalid sip address"));
    return res;
  }
  pj_strset2(&sip_address, scaip_msg->address);
  pj_strset2(&sip_msg, scaip_msg->msg);

  PJ_LOG(5, (THIS_FILE, "Sending SCAIP IM"));
  PJ_LOG(5, (THIS_FILE, "  sip address: %s", sip_address));
  PJ_LOG(5, (THIS_FILE, "  sip message: %s", sip_msg));

	res = pjsua_im_send(current_acc, &sip_address, &mime, &sip_msg, NULL, NULL);

  /* let them know how it went. */
  //ssapmsg_response_send(res, NULL);

  return res;
}

void ui_handle_scaip_cmd(const char* const inp) {
	switch(inp[0]) {
		case 'i':
		  PJ_LOG(2, (THIS_FILE, "Sending IM message: %s", inp +1));
		  send_scaip_im_str(inp +1);
		  break;

		case 'm':
		  PJ_LOG(2, (THIS_FILE, "Calling: %s", inp +1));
		  make_scaip_call(inp +1);
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
const char* ui_scaip_handler(const struct ssapmsg_iface* const msg) {
  const char* parent_cmd = NULL;
  PJ_LOG(3, (THIS_FILE, "Scaip command is %d", msg->type));

  switch( msg->type ) {
    case SSAPMSG_UNDEFINED:
      PJ_LOG(3, (THIS_FILE, "Scaip command is undefined. Did you mean to send this?"));
      break;
    case SSAPMSG_CALL:
      PJ_LOG(3, (THIS_FILE, "Calling someone (NOT IMPLEMENTED)"));
      //PJ_LOG(2, (THIS_FILE, "Calling: %s", inp+1));
      //make_scaip_call(pj_strbuf(&arg));
      /* TODO: Find a place for this:
      {
        const int i = pjsua_call_get_count();
        PJ_LOG(2, (THIS_FILE, "Number of calls: %d", i));
        data_output("%d", i);
      }*/
      break;
    case SSAPMSG_SCAIPMSG:
      //PJ_LOG(3, (THIS_FILE, "Sending a message (NOT IMPLEMENTED)"));
      PJ_LOG(2, (THIS_FILE, "Sending IM message: %s", msg->address));
      send_scaip_im(msg);
      break;
    case SSAPMSG_CONFIG:
      PJ_LOG(3, (THIS_FILE, "CONFIG scaip command. (NOT IMPLEMENTED)"));
      break;
    case SSAPMSG_PJSUA:
      PJ_LOG(3, (THIS_FILE, "Passing command to pjsua legacy app."));
      //parent_cmd = &data->pjsua.data[0];
      //parent_cmd = msg->pjsua.data;
      parent_cmd = msg->data;
      break;
    case SSAPMSG_WARNING: /* fall-through */
    case SSAPMSG_ERROR:
		  PJ_LOG(3, (THIS_FILE, "Parse error of received packet."));
      break;
    default:
      PJ_LOG(3, (THIS_FILE, "Unknown scaip command: %d", msg->type));
      break;
  }

  return parent_cmd;
}
#endif
