#ifndef _PJSUA_DP_H_
#define _PJSUA_DP_H_


#define DEFAULT_DP_PORT (8890)

#include <pj/types.h>

pj_status_t start_ssap_iface(void);
pj_status_t update_ssap_iface(void);
pj_status_t close_ssap_connection(void);
pj_status_t reset_ssap_connection(void);
pj_status_t reset_ssap_iface(void);
pj_status_t teardown_ssap_iface(void);

pj_status_t dp_receive(char* const inp, pj_ssize_t lim);
//pj_status_t dp_receive_block(char* const inp, pj_ssize_t lim);
//pj_status_t receive_handler(char* const inp, pj_ssize_t lim);
int dp_send(const void* const data, pj_ssize_t len);
void ui_scaip_handler(const char* const inp);
void ui_scaip_keystroke_help(void);


#endif

