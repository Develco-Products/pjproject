#ifndef _PJSUA_SSAPCOMM_H_
#define _PJSUA_SSAPCOMM_H_

#define MANUAL_SETTINGS
#define UI_SOCKET (0)
#define UI_TERMINAL (1)
#define ENABLE_PJSUA_SSAP (0)

/* Log data output. Value > 0 specifies log level. 
 * 0 disables. */
#define DATA_OUTPUT_LOG_LEVEL  (3)

#define DEFAULT_DP_PORT (8890)
#if !defined(MANUAL_SETTINGS)
#	if defined(__arm__)
#	 	define ENABLE_PJSUA_SSAP (1)
#		define UI_SOCKET (1)
#		define UI_TERMINAL (0)
#	else
#	 	define ENABLE_PJSUA_SSAP (0)
#		define UI_SOCKET (0)
#		define UI_TERMINAL (1)
#	endif
#endif

//#define DISABLE_PJSUA_UI (1)

/* If SSAP module has been enabled, disable UI interface. */
#if ENABLE_PJSUA_SSAP
#	if UI_TERMINAL
#		error SSAP and terminal input cannot be enabled at the same time.
#	endif
#	if ENABLE_PJSUA_SSAP
#		if defined(DISABLE_PJSUA_UI)
#			undef DISABLE_PJSUA_UI
#			define DISABLE_PJSUA_UI (1)
#		endif
#	endif
#else
#	define ENABLE_PJSUA_SSAP (0)
#endif

#include <pj/types.h>
#include "pjsua_ssapmsg.h"

pj_status_t start_ssap_iface(void);
pj_status_t update_ssap_iface(void);
pj_status_t close_ssap_connection(void);
pj_status_t reset_ssap_connection(void);
pj_status_t reset_ssap_iface(void);
pj_status_t teardown_ssap_iface(void);

pj_status_t ssapsock_receive(uint8_t* const inp, pj_ssize_t* lim);
pj_status_t ssapsock_send(const void* const data, pj_ssize_t* len);
void ssapsock_send_blind(const void* const data, pj_ssize_t len);
#if ENABLE_PJSUA_SSAP
//const char* ui_scaip_handler(const enum ssapmsg_type msg_type, const union ssapmsg_payload* const data);
const char* ui_scaip_handler(const struct ssapmsg_iface* const msg);
#endif
void ui_scaip_handler_str_input(const char* const inp);
void ui_scaip_keystroke_help(void);

#if UI_SOCKET
extern char dp_print_buffer[1024];
#	if ENABLE_PJSUA_SSAP
#		define printf(...) while(0)
#		define puts(s) while(0)
#		define data_output(...) {sprintf(dp_print_buffer, __VA_ARGS__);ssapsock_send_blind(dp_print_buffer, strlen(dp_print_buffer));}while(0)
#	else
# 	define printf(...) {sprintf(dp_print_buffer, __VA_ARGS__);ssapsock_send_blind(dp_print_buffer, strlen(dp_print_buffer));}while(0)
# 	define puts(s) {sprintf(dp_print_buffer, "%s\n", s);ssapsock_send_blind(dp_print_buffer, strlen(dp_print_buffer));}while(0)
#		define data_output(...) {sprintf(dp_print_buffer, __VA_ARGS__);ssapsock_send_blind(dp_print_buffer, strlen(dp_print_buffer));}while(0)
#	endif
#	define ui_input(buffer, buffer_length) ui_input_socket(buffer, buffer_length)
pj_ssize_t ui_input_socket(char* const buf, pj_size_t len);
#elif UI_TERMINAL
# define printf(...) {printf(__VA_ARGS__);fflush(stdout);}while(0)
# define puts(s) {puts(s);fflush(stdout);}while(0)
#	define ui_input(buffer, buffer_length) ui_input_terminal(buffer, buffer_length)
#	define data_output(...) {printf(__VA_ARGS__);fflush(stdout);}while(0)
pj_ssize_t ui_input_terminal(char* const buf, pj_size_t len);
#else
# error "You must specify where UI i/o is sent."
#endif

#if DATA_OUTPUT_LOG_LEVEL == 0
# define LOG_DATA_OUTPUT(s) 
#elif DATA_OUTPUT_LOG_LEVEL == 1
# define LOG_DATA_OUTPUT(s) PJ_LOG(1,(THIS_FILE,"%s",s));
#elif DATA_OUTPUT_LOG_LEVEL == 2
# define LOG_DATA_OUTPUT(s) PJ_LOG(2,(THIS_FILE,"%s",s));
#elif DATA_OUTPUT_LOG_LEVEL == 3
# define LOG_DATA_OUTPUT(s) PJ_LOG(3,(THIS_FILE,"%s",s));
#elif DATA_OUTPUT_LOG_LEVEL == 4
# define LOG_DATA_OUTPUT(s) PJ_LOG(4,(THIS_FILE,"%s",s));
#elif DATA_OUTPUT_LOG_LEVEL == 5
# define LOG_DATA_OUTPUT(s) PJ_LOG(5,(THIS_FILE,"%s",s));
#elif DATA_OUTPUT_LOG_LEVEL == 6
# define LOG_DATA_OUTPUT(s) PJ_LOG(6,(THIS_FILE,"%s",s));
#else
# error data log level invalid.
#endif

#endif /* _PJSUA_SSAPCOMM_H_ */

