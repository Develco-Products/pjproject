#ifndef _PJSUA_DP_H_
#define _PJSUA_DP_H_


#define DEFAULT_DP_PORT (8890)
#define UI_SOCKET (0)
#define UI_TERMINAL (1)


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

#if UI_SOCKET
extern char dp_print_buffer[1024];
# define printf(...) {sprintf(dp_print_buffer, __VA_ARGS__);dp_send(dp_print_buffer, strlen(dp_print_buffer));}
# define puts(s) {sprintf(dp_print_buffer, "%s\n", s);dp_send(dp_print_buffer, strlen(dp_print_buffer));}
#	define ui_input(buffer, buffer_length) ui_input_socket(buffer, buffer_length)
pj_ssize_t ui_input_socket(char* const buf, pj_size_t len);
pj_status_t ui_outp_socket(const char* const msg, pj_ssize_t len);
#elif UI_TERMINAL
# define printf(...) {printf(__VA_ARGS__);fflush(stdout);}
# define puts(s) {puts(s);fflush(stdout);}
#	define ui_input(buffer, buffer_length) ui_input_terminal(buffer, buffer_length)
pj_ssize_t ui_input_terminal(char* const buf, pj_size_t len);
#else
# error "You must specify where UI i/o is sent."
#endif

#endif

