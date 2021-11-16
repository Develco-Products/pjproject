#ifndef _PJSUA_APP_SCAIP_H_
#define _PJSUA_APP_SCAIP_H_


void ui_handle_scaip_cmd(const char* const inp);

#if ENABLE_PJSUA_SSAP
pj_status_t ui_scaip_handler(const struct ssapmsg_iface* const msg, char** app_cmd);
#endif

#endif /* _PJSUA_APP_SCAIP_H_ */

