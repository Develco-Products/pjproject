
#include <pjsua-lib/pjsua.h>
#include "pjsua_app_common.h"

#include "pjsua_dp.h"
#include "pjlib.h"
#include "pjsua_app.h"
#include <string.h>
#include "pj/types.h"

#define THIS_FILE "pjsua_dp.c"

/* An attempt to avoid stdout buffering for python tests:
 * - call 'fflush(stdout)' after each call to 'printf()/puts()'
 * - apply 'setbuf(stdout, 0)', but it is not guaranteed by the standard:
 *   http://stackoverflow.com/questions/1716296
 */
#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) || \
  (defined (_MSC_VER) && _MSC_VER >= 1400)
/* Variadic macro is introduced in C99; MSVC supports it in since 2005. */
#  define printf(...) {printf(__VA_ARGS__);fflush(stdout);}
#  define puts(s) {puts(s);fflush(stdout);}
#endif

#define log_debug(msg)  PJ_LOG(5, (THIS_FILE, msg))
#define log_info(msg)   PJ_LOG(5, (THIS_FILE, msg))
#define log_warn(msg)   PJ_LOG(3, (THIS_FILE, msg))
#define log_err(msg)    PJ_LOG(2, (THIS_FILE, msg))
#define log_crit(msg)   PJ_LOG(1, (THIS_FILE, msg))

//int connection_handler( void* socket_desc );
static int connection_handler( pj_sock_t* sock );
static int send_scaip_im(const char* const sip_address);
static int make_scaip_call(const char* const call_data);

static pj_sock_t socket_desc;
static pj_sock_t sock;
static pj_sockaddr_in server_addr;

int dp_init(void)
{
  int res = setup_server(8890);
  if( res != 0 ) {
    log_err("Could not start dp server. Abort.");
  }

  return res;
}


int setup_server(int port) {
  log_debug("debug Starting DP socket.");
  log_info("info Starting DP socket.");
  log_warn("warn Starting DP socket.");
  log_err("err Starting DP socket.");
  log_crit("crit Starting DP socket.");

  if( port == 0 ) {
    port = DEFAULT_DP_PORT;
  }

  // create socket
  log_debug("Creating socket.");
  pj_status_t res = pj_sock_socket( pj_AF_INET(), pj_SOCK_STREAM(), 0, &socket_desc );
  if( res != 0 ) {
    log_err("Could not create socket.");
    return 1;
  }

  server_addr.sin_family = PJ_AF_INET;
  server_addr.sin_addr.s_addr = PJ_INADDR_ANY;
  server_addr.sin_port = pj_htons( port );

  // bind
  log_debug("Attempting to bind socket.");
  if( pj_sock_bind( socket_desc, &server_addr, sizeof(server_addr) ) < 0 ) {
    log_err("Could not bind socket.");
    return 1;
  }
  log_debug("Bind successful.");

  log_debug("Listening for connections.");
  res = pj_sock_listen( socket_desc, 2 );
  if( res != 0 ) {
      log_err("Could not initiate listening.");
      return 1;
  }

  return 0;
}


int wait_for_connection(void) {
  int c = sizeof( pj_sockaddr_in );
  pj_sockaddr_in client;
  pj_str_t msg_buffer;

  log_info("Waiting for client connection...");
  pj_status_t res = pj_sock_accept( socket_desc, &sock, &client, &c );
  if( res != 0 ) {
    log_warn("Failed to get client connection.");
  }

  return res;
}

int dp_receive(char* const inp, pj_size_t lim) {
  pj_status_t res = pj_sock_recv(sock, inp, &lim, 0);
  if( res == 0 ) {
    inp[lim] = '\0';
  }
  else {
    log_warn("Could not receive data from client.");
  }

  return res;
}

int dp_receive_block(char* const inp, pj_size_t lim) {
  pj_status_t res = 0;
  pj_size_t s = lim;
  pj_size_t offset = 0;

  res = dp_receive(inp, s);
  while( res == 0 && (s < lim || inp[s-1] != '\0') ) {
    pj_thread_sleep(500);
    offset = s;
    s = lim;
    res = dp_receive(inp + offset, s);
  }

  if( res != 0 ) {
    PJ_LOG(5, (THIS_FILE, "receive returned: %d", res));
  }

  return res;
}

int dp_send(const void* const data, pj_ssize_t len) {
  pj_status_t res = pj_sock_send(sock, data, &len, 0);
  if( res != PJ_SUCCESS ) {
    log_warn("Failure transmitting data to host.");
  }
  return res;
}

void ui_scaip_handler(const char* const inp) {
  char c = inp[0];
  pj_str_t arg;
  pj_cstr(&arg, inp +1);
  pj_strtrim( &arg );

  switch( c ) {
    case 'i':
      send_scaip_im(pj_strbuf(&arg));
      break;
    case 'm':
      make_scaip_call(pj_strbuf(&arg));
      break;
    case '\0':
      /*Empty command. Just ignore.*/
      break;
    default:
      log_info("Unknown scaip command.");
      break;
  }
}


int teardown_server(void) {
  log_debug("Tearing down socket.");
  pj_status_t res = pj_sock_close(socket_desc);
  if( res != 0 ) {
    PJ_LOG(3, (THIS_FILE, "Could not close socket descriptor."));
  }
}

void ui_scaip_keystroke_help(void) {
  const char help_text[] = \
    "\n" \
    "+=============================================================================+\n" \
    "|       SCAIP commands:                                                       |\n" \
    "|                                                                             |\n" \
    "| !h                This help.                                                |\n" \
    "| !i <addr> <msg>   Send SCAIP message. msg must be xml and message is sent   |\n" \
    "|                   with mime type application/scaip+xml.                     |\n" \
    "| !m <addr>         Call SIP addr.                                            |\n" \
    "+=============================================================================+\n";

  printf("%s", help_text);
  dp_send(help_text, strlen(help_text));
}



static int send_scaip_im(const char* const scaip_msg) {
  char recv_buffer[1024];
  int res;
  pj_str_t mime = pj_str("application/scaip+xml");

  pj_str_t sip_address;
  pj_str_t sip_msg;
  int c;

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
  if( pjsua_verify_url(sip_address_buffer) != PJ_SUCCESS ) {
    log_warn("Invalid sip address.");
    return 1;
  }
  pj_strset2( &sip_address, sip_address_buffer );

  PJ_LOG(5, (THIS_FILE, "sip address: %s", sip_address));
  PJ_LOG(5, (THIS_FILE, "sip message: %s", sip_msg));

	pjsua_im_send(current_acc, &sip_address, &mime, &sip_msg, NULL, NULL);

  return 0;
}

static int make_scaip_call(const char* const call_data) {
  pj_status_t res;
  pj_str_t sip_address;

  pj_cstr(&sip_address, call_data);
  pj_strtrim(&sip_address);

  PJ_LOG(5, (THIS_FILE, "Verifying sip address: %s", sip_address));
  if( pjsua_verify_url(pj_strbuf(&sip_address)) != PJ_SUCCESS ) {
    log_warn("Invalid sip address.");
    return 1;
  }

	res = pjsua_call_make_call(current_acc, &sip_address, &call_opt, NULL,
	    NULL, &current_call);

  return (res == PJ_SUCCESS) ? 0 : 1;
}

//int connection_handler( void* socket_desc ) {
static int connection_handler( pj_sock_t* sock ) {
  //pj_sock_t sock = *(pj_sock_t*) socket_desc;

  pj_str_t msg = pj_str("This is a message from the string handler. This means it is working.");
  pj_ssize_t l = pj_strlen(&msg);
  PJ_LOG(5, (THIS_FILE, "Sending message (%d): %s", pj_strlen(&msg), msg));
  //pj_status_t res = pj_sock_send(sock, &msg, &l, 0);
  pj_status_t res = pj_sock_send(*sock, pj_strbuf(&msg), &l, 0);
  if( res == 0 ) {
    PJ_LOG(5, (THIS_FILE, "Message sent, len: %lu.", l));
  }
  else {
    PJ_LOG(3, (THIS_FILE, "Failed sending message."));
  }

#if 1
  msg = pj_str("Another message. Look at how awesome this is.");
  l = pj_strlen(&msg);
  //res = pj_sock_send(sock, &msg, &l, 0);
  res = pj_sock_send(*sock, pj_strbuf(&msg), &l, 0);
  PJ_LOG(5, (THIS_FILE, "Sending message (%d): %s", pj_strlen(&msg), msg));
  if( res == 0 ) {
    PJ_LOG(5, (THIS_FILE, "Message sent, len: %lu.", l));
  }
  else {
    PJ_LOG(3, (THIS_FILE, "Failed sending message."));
  }
#endif
  //pj_thread_sleep(1000);

  log_debug("Closing socket.");
  res = pj_sock_shutdown(*sock, PJ_SHUT_RDWR);
  if( res != 0 ) {
      PJ_LOG(3, (THIS_FILE, "Could not close connection."));
  }

  return 0;
}

