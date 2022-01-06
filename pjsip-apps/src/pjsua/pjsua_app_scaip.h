#ifndef _PJSUA_APP_SCAIP_H_
#define _PJSUA_APP_SCAIP_H_

#include "pjsua_ssapmsg.h"

void ui_handle_scaip_cmd(const char* const inp);

#if ENABLE_PJSUA_SSAP
pj_status_t ui_scaip_handler(const ssapmsg_datagram_t* const msg, pj_str_t* app_cmd);
pj_status_t ui_scaip_account_add(const ssapconfig_account_add_t* const acc,
    const pjsua_transport_config* const rtp_cfg, pjsua_acc_id* acc_id);
#endif

#endif /* _PJSUA_APP_SCAIP_H_ */

