#ifndef _PJSUA_APP_SCAIP_H_
#define _PJSUA_APP_SCAIP_H_


void ui_handle_scaip_cmd(const char* const inp);

#if ENABLE_PJSUA_SSAP
const char* ui_scaip_handler(const struct ssapmsg_iface* const msg);
#endif

#endif /* _PJSUA_APP_SCAIP_H_ */

