#ifndef _PJSUA_DP_H_
#define _PJSUA_DP_H_


#define DEFAULT_DP_PORT (8890)

#include <pj/types.h>

int dp_init(void);
int setup_server(int port);
int wait_for_connection(void);
int dp_receive(char* const inp, pj_size_t lim);
int dp_receive_block(char* const inp, pj_size_t lim);
int dp_send(const void* const data, pj_ssize_t len);
void ui_scaip_handler(const char* const inp);
int teardown_server(void);
void ui_scaip_keystroke_help(void);


#endif

