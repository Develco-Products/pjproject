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
//static pj_status_t make_scaip_call(const ssapmsg_datagram_t* const call_data);
static pj_status_t make_scaip_call(const ssapcall_dial_t* const call_dial);
static pj_status_t send_scaip_im_str(const char* const scaip_msg);
//static pj_status_t send_scaip_im(const ssapmsg_datagram_t* const scaip_msg);
static pj_status_t send_scaip_im(const ssapmsg_scaip_t* const scaip_msg);
static pj_status_t send_call_status(const ssapmsg_datagram_t* const msg);

static pj_status_t make_scaip_call_str(const char* const call_data) {
  pj_status_t res;
  pj_str_t sip_address;

  pj_cstr(&sip_address, call_data);
  pj_strtrim(&sip_address);

  PJ_LOG(3, (THIS_FILE, "Verifying sip address: %s", pj_strbuf(&sip_address)));
  if((res = pjsua_verify_url(pj_strbuf(&sip_address))) != PJ_SUCCESS ) {
    ssapmsg_status_t response = {
        .code = res,
        .msg = pj_str("Invalid sip address")
    };

    PJ_PERROR(3, (THIS_FILE, res, pj_strbuf(&response.msg)));
    ssapmsg_send_reply(SSAPMSG_NACK, &response);
    return res;
  }

	res = pjsua_call_make_call(current_acc, &sip_address, &call_opt, NULL,
	    NULL, &current_call);

  /* let them know how it went. */
  ssapmsg_response(res, NULL);

  return res;
}

static pj_status_t make_scaip_call(const ssapcall_dial_t* const call_dial) {
  pj_status_t res;

  PJ_LOG(3, (THIS_FILE, "Verifying sip address: %s", pj_strbuf(&call_dial->sip_address)));
  if((res = pjsua_verify_url(pj_strbuf(&call_dial->sip_address))) != PJ_SUCCESS ) {
    ssapmsg_status_t response = { .code = res, .msg = pj_str("Invalid sip address") };
    PJ_PERROR(3, (THIS_FILE, res, pj_strbuf(&response.msg)));
    ssapmsg_send_reply(SSAPMSG_NACK, &response);
    return res;
  }

	res = pjsua_call_make_call(current_acc, &call_dial->sip_address, &call_opt, NULL,
	    NULL, &current_call);

  /* let them know how it went. */
  //ssapmsg_response(res, NULL);

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

static pj_status_t send_scaip_im(const ssapmsg_scaip_t* const scaip_msg) {
  pj_str_t mime = pj_str("application/scaip+xml");
  pj_status_t res;

  PJ_LOG(5, (THIS_FILE, "Verifying sip address: %s", pj_strbuf(&scaip_msg->receiver_url)));
  if((res = pjsua_verify_url(pj_strbuf(&scaip_msg->receiver_url))) != PJ_SUCCESS ) {
    PJ_PERROR(3, (THIS_FILE, res, "Invalid sip address"));
    return res;
  }

  PJ_LOG(5, (THIS_FILE, "Sending SCAIP IM"));
  PJ_LOG(5, (THIS_FILE, "  sip address: %s", pj_strbuf(&scaip_msg->receiver_url)));
  PJ_LOG(5, (THIS_FILE, "  sip message: %s", pj_strbuf(&scaip_msg->msg)));

	res = pjsua_im_send(current_acc, &scaip_msg->receiver_url, &mime, &scaip_msg->msg, NULL, NULL);

  return res;
}

static pj_status_t send_call_status(const ssapmsg_datagram_t* const msg) {
  pjsua_call_info cinfo;
  pjsua_call_get_info(msg->payload.call_status.id, &cinfo);
  return ssapmsg_send_status(SSAPCALL_STATUS, &cinfo);
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

pj_status_t ui_scaip_account_add(const ssapconfig_account_add_t* const acc,
    const pjsua_transport_config* const rtp_cfg, pjsua_acc_id* acc_id) {
  pjsua_acc_config acc_cfg;

  pjsua_acc_config_default(&acc_cfg);
  acc_cfg.id = acc->sip_address;
  acc_cfg.reg_uri = acc->registrar;
  acc_cfg.cred_count = 1;
  acc_cfg.cred_info[0].scheme = pj_str("Digest");
  acc_cfg.cred_info[0].realm = acc->auth_realm;
  acc_cfg.cred_info[0].username = acc->auth_user;
  acc_cfg.cred_info[0].data_type = 0;
  acc_cfg.cred_info[0].data = acc->auth_pass;
  acc_cfg.rtp_cfg = *rtp_cfg;
  app_config_init_video(&acc_cfg);

  pj_status_t res = pjsua_acc_add(&acc_cfg, PJ_TRUE, acc_id);

  if(res != PJ_SUCCESS) {
    PJ_PERROR(3, (THIS_FILE, res, "Could not add account"));
  }

  return res;
}

pj_status_t ui_scaip_account_del(const ssapconfig_account_del_t* const acc) {
  if(pjsua_acc_is_valid(acc->id)) {
    pjsua_acc_del(acc->id);
    return PJ_SUCCESS;
  }
  else {
    PJ_LOG(3, (THIS_FILE, "Invalid account ID. Cannot remove."));
    return PJ_EINVAL;
  }
}

pj_status_t ui_scaip_account_list(ssapconfig_account_list_t* const acc) {
  acc->acc_no = PJSUA_MAX_ACC;
  return pjsua_enum_accs(acc->acc, &acc->acc_no);
}

pj_status_t ui_scaip_account_info(pjsua_acc_info* const acc) {
  if(pjsua_acc_is_valid(acc->id)) {
    const pjsua_acc_id acc_id = acc->id;
    pjsua_acc_get_info(acc_id, acc);

    return PJ_SUCCESS;
  }
  else {
    return PJ_EINVAL;
  }
}

pj_status_t ui_scaip_buddy_add(const ssapconfig_buddy_add_t* const buddy_add) {
  pjsua_buddy_id id;
  pjsua_buddy_config cfg;
  pj_status_t res;
  const char* const sip_url = pj_strbuf(&buddy_add->sip_url);

  res = pjsua_verify_url(sip_url);
  if(res == PJ_SUCCESS) {
    pj_bzero(&cfg, sizeof(pjsua_buddy_config));
    pj_cstr(&cfg.uri, sip_url);
    cfg.subscribe = PJ_TRUE;
  
    res = pjsua_buddy_add(&cfg, &id);
  }
  else {
    PJ_LOG(3, (THIS_FILE, "Invalid sip address: %s", sip_url));
  }

  return res;
}

pj_status_t ui_scaip_buddy_del(const ssapconfig_buddy_del_t* const buddy_del) {
  if(pjsua_buddy_is_valid(buddy_del->id)) {
    pjsua_buddy_del(buddy_del->id);
    return PJ_SUCCESS;
  }
  else {
    PJ_LOG(3, (THIS_FILE, "Invalid buddy ID: %d", buddy_del->id));
    return PJ_EINVAL;
  }
}

pj_status_t ui_scaip_buddy_list(ssapconfig_buddy_list_t* const buddy_list) {
  buddy_list->buddy_no = PJSUA_MAX_BUDDIES;
  return pjsua_enum_buddies(buddy_list->buddy, &buddy_list->buddy_no);
}

pj_status_t ui_scaip_buddy_info(pjsua_buddy_info* const buddy_info) {
  const pjsua_buddy_id id = buddy_info->id;
  pj_bool_t is_valid = pjsua_buddy_is_valid(id);

  if(is_valid) {
    pj_bzero(buddy_info, sizeof(pjsua_buddy_info));
    return pjsua_buddy_get_info(id, buddy_info);
  }
  else {
    PJ_LOG(3, (THIS_FILE, "Invalid buddy ID: %d", id));
    return PJ_EINVAL;
  }
}

#if ENABLE_PJSUA_SSAP
pj_status_t ui_scaip_handler(const ssapmsg_datagram_t* const msg, pj_str_t* app_cmd) {
  pj_status_t res = -1;
  PJ_LOG(3, (THIS_FILE, "Scaip command is 0x%04X", msg->type));

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
      {
        ssapcall_dial_t call_dial; 
        res = ssapmsg_unpack(&call_dial, msg);
        if(res != PJ_SUCCESS) {
          PJ_PERROR(3, (THIS_FILE, res, "Could not unpack %s", ssapmsgtype_str(msg->type)));
          return res;
        }

        //PJ_LOG(2, (THIS_FILE, "Dialling a new SIP call to: %s", ssapmsg_scaip_address(msg)));
        res = make_scaip_call(&call_dial);
        ssapmsg_response(res, NULL);
        break;
      }
	  case SSAPCALL_ANSWER:
	  case SSAPCALL_HANGUP:
      PJ_LOG(3, (THIS_FILE, "functionlity NOT IMPLEMENTED for %s", ssapmsgtype_str(msg->type)));
      break;

		case SSAPCALL_STATUS:
      if(ssapmsg_is_status(msg)) {
        PJ_LOG(3, (THIS_FILE, "Call status has no meaning as a status message. Dropping message."));
        res = PJ_SUCCESS;
      }
      else {
        pjsua_call_info cinfo;
        res = ssapmsg_unpack(&cinfo, msg);
        if(res != PJ_SUCCESS) {
          PJ_PERROR(3, (THIS_FILE, res, "Could not unpack %s", ssapmsgtype_str(msg->type)));
          return res;
        }

        PJ_LOG(3, (THIS_FILE, "Getting call status for call id: %d", cinfo.id));
        if(cinfo.id < pjsua_call_get_max_count()) {
          res = pjsua_call_get_info(cinfo.id, &cinfo);
          if(res == PJ_SUCCESS) {
            res = ssapmsg_send_status(SSAPCALL_STATUS, &cinfo);
          }
          else {
            res = ssapmsg_response(res, "Failed to obtain call info.");
          }
        }
        else {
          res = ssapmsg_response(PJ_EINVAL, "Call id is invalid.");
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
      {
        ssapmsg_scaip_t scaip_msg;
        res = ssapmsg_unpack(&scaip_msg, msg);
        if(res != PJ_SUCCESS) {
          PJ_PERROR(3, (THIS_FILE, res, "Could not unpack %s", ssapmsgtype_str(msg->type)));
          return res;
        }

        //PJ_LOG(2, (THIS_FILE, "Sending IM message: %s", ssapmsg_scaip_address(msg)));
        res = send_scaip_im(&scaip_msg);
        break;
      }
		case SSAPMSG_DTMF:
	  case SSAPMSG_PLAIN:
      PJ_LOG(3, (THIS_FILE, "functionlity NOT IMPLEMENTED for %s", ssapmsgtype_str(msg->type)));
      break;


	  case SSAPCONFIG_QUIT:
      PJ_LOG(2, (THIS_FILE, "Quitting app."));

      /* Send response that message was received. */
      ssapmsg_response(PJ_SUCCESS, NULL);
      pj_thread_sleep(500);

      legacy_on_stopped(PJ_FALSE);
      res = PJ_SUCCESS;
      break;

	  case SSAPCONFIG_RELOAD:
      PJ_LOG(2, (THIS_FILE, "Reloading app."));

      /* Send response that message was received. */
      ssapmsg_response(PJ_SUCCESS, NULL);
      pj_thread_sleep(500);

      legacy_on_stopped(PJ_TRUE);
      res = PJ_SUCCESS;
      break;
      
	  case SSAPCONFIG_STATUS:
      PJ_LOG(3, (THIS_FILE, "functionlity NOT IMPLEMENTED for %s", ssapmsgtype_str(msg->type)));
      break;

		case SSAPCONFIG_ACCOUNT_ADD:
      {
        ssapconfig_account_add_t acc_add;
        pjsua_acc_info acc_info;
        res = ssapmsg_unpack(&acc_add, msg);
        if(res != PJ_SUCCESS) { return res; }

        PJ_LOG(3, (THIS_FILE, "Adding account: %s", pj_strbuf(&acc_add.sip_address)));
        res = ui_scaip_account_add(&acc_add, &app_config.rtp_cfg, &acc_info.id);

        if(res == PJ_SUCCESS) {
          // Collect acc. info and return that. */
          res = ui_scaip_account_info(&acc_info);
          res = (res == PJ_SUCCESS) 
            ? ssapmsg_send_reply(SSAPCONFIG_ACCOUNT_INFO, &acc_info)
            : ssapmsg_response(res, "Added account but subsequent account info fetch failed.");
        }
        else {
          res = ssapmsg_response(res, NULL);
        }
        break;
      }
		case SSAPCONFIG_ACCOUNT_DELETE:
      {
        ssapconfig_account_del_t acc_del;
        res = ssapmsg_unpack(&acc_del, msg);
        if(res != PJ_SUCCESS) { return res; }

        PJ_LOG(3, (THIS_FILE, "Delete account: %d", acc_del.id));
        res = ui_scaip_account_del(&acc_del);
        res = ssapmsg_response(res, "Failed to delete account.");
        break;
      }
		case SSAPCONFIG_ACCOUNT_LIST:
      PJ_LOG(3, (THIS_FILE, "List accounts"));
      if(ssapmsg_is_status(msg)) {
        PJ_LOG(3, (THIS_FILE, "%s has no meaning as a status message. Dropping message.", ssapmsgtype_str(msg->type)));
        res = PJ_SUCCESS;
      }
      else {
        ssapconfig_account_list_t acc_list;

        res = ui_scaip_account_list(&acc_list);

        res = (res == PJ_SUCCESS)
          ? ssapmsg_send_reply(SSAPCONFIG_ACCOUNT_LIST, &acc_list)
          : ssapmsg_response(res, NULL);
      }
      break;
		case SSAPCONFIG_ACCOUNT_INFO:
      {
        pjsua_acc_info acc_info;
        res = ssapmsg_unpack(&acc_info, msg);
        if(res != PJ_SUCCESS) { return res; }

        PJ_LOG(3, (THIS_FILE, "Fetch account %d info.", acc_info.id));
        res = ui_scaip_account_info(&acc_info);

        res = (res == PJ_SUCCESS)
          ? ssapmsg_send_reply(SSAPCONFIG_ACCOUNT_INFO, &acc_info)
          : ssapmsg_response(res, NULL);
        break;
      }
		case SSAPCONFIG_BUDDY_ADD:
      {
        ssapconfig_buddy_add_t buddy_add;
        res = ssapmsg_unpack(&buddy_add, msg);
        if(res != PJ_SUCCESS) { return res; }

        PJ_LOG(3, (THIS_FILE, "Adding buddy: %s", pj_strbuf(&buddy_add.sip_url)));
        res = ui_scaip_buddy_add(&buddy_add);

        /* Send send result. */
        res = ssapmsg_response(res, NULL);
        break;
      }
		case SSAPCONFIG_BUDDY_DELETE:
      {
        ssapconfig_buddy_del_t buddy_del;
        res = ssapmsg_unpack(&buddy_del, msg);
        if(res != PJ_SUCCESS) { return res; }
        
        PJ_LOG(3, (THIS_FILE, "Removing buddy: %d", buddy_del.id));
        res = ui_scaip_buddy_del(&buddy_del);

        res = ssapmsg_response(res, "Invalid buddy ID");
        break;
      }
		case SSAPCONFIG_BUDDY_LIST:
      PJ_LOG(3, (THIS_FILE, "List buddies"));
      if(ssapmsg_is_status(msg)) {
        PJ_LOG(3, (THIS_FILE, "%s has no meaning as a status message. Dropping message.", ssapmsgtype_str(msg->type)));
        res = PJ_SUCCESS;
      }
      else {
        ssapconfig_buddy_list_t buddy_list;

        res = ui_scaip_buddy_list(&buddy_list);

        res = (res == PJ_SUCCESS) 
          ? ssapmsg_send_reply(SSAPCONFIG_BUDDY_LIST, &buddy_list)
          : ssapmsg_response(res, NULL);
      }
      break;
		case SSAPCONFIG_BUDDY_INFO:
      if(ssapmsg_is_status(msg)) {
        PJ_LOG(3, (THIS_FILE, "%s has no meaning as a status message. Dropping message.", ssapmsgtype_str(msg->type)));
        res = PJ_SUCCESS;
      }
      else {
        pjsua_buddy_info buddy_info;
        res = ssapmsg_unpack(&buddy_info, msg);
        if(res != PJ_SUCCESS) { return res; }

        PJ_LOG(3, (THIS_FILE, "Buddy info for id: %d", buddy_info.id));
        res = ui_scaip_buddy_info(&buddy_info);
        
        res = (res == PJ_SUCCESS) 
          ? ssapmsg_send_reply(SSAPCONFIG_BUDDY_INFO, &buddy_info)
          : ssapmsg_response(res, "Failed to find info for buddy id.");
      }
      break;

	  case SSAPPJSUA:
      {
        PJ_LOG(2, (THIS_FILE, "legacy %s command.", ssapmsgtype_str(msg->type)));
        ssappjsua_pjsua_t pjsua;
        res = ssapmsg_unpack(&pjsua, msg);
        if(res != PJ_SUCCESS) { return res; }

        if(res == PJ_SUCCESS) {
          *app_cmd = pjsua.pjsua_cmd;
        }

        if(!ssapmsg_is_status(msg)) {
          res = ssapmsg_response(res, NULL);
        }
        break;
      }

    default:
      PJ_LOG(3, (THIS_FILE, "Unknown scaip command: %d", msg->type));
      res = PJ_EINVAL;
      break;
  }

  return res;
}
#endif

