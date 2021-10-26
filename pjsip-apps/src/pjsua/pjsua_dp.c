
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
//#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) || \
//  (defined (_MSC_VER) && _MSC_VER >= 1400)
///* Variadic macro is introduced in C99; MSVC supports it in since 2005. */
//#  define printf(...) {printf(__VA_ARGS__);fflush(stdout);}
//#  define puts(s) {puts(s);fflush(stdout);}
//#endif

static pj_status_t send_scaip_im(const char* const sip_address);
static pj_status_t make_scaip_call(const char* const call_data);

/* socket descriptors */
static pj_sock_t host_socket;
static pj_sock_t client_socket;

static pj_sockaddr_in server_addr;
static pj_uint16_t listening_port = DEFAULT_DP_PORT;
static enum conn_state {
  CONN_STATE_NOT_INITIALIZED,
  CONN_STATE_NOT_CONNECTED,
  CONN_STATE_LISTENING,
  CONN_STATE_WAITING,
  CONN_STATE_CONNECTED,
  CONN_STATE_ERROR
} conn_state = CONN_STATE_NOT_INITIALIZED;

#if UI_SOCKET
char dp_print_buffer[1024];
pj_ssize_t ui_input_socket(char* const buf, pj_size_t len)
{
	pj_status_t res = dp_receive(buf, len);

	if(res == PJ_SUCCESS) {
		return strlen(buf);
	}
	else {
		PJ_PERROR(3, (THIS_FILE, res, "  (%s) reading data from socket: ", __func__));
		return -1;
	}
}

#if 0
pj_status_t ui_outp_socket(const char* const msg, pj_ssize_t len) {
	return dp_send(msg, len);
}
#endif
#endif

#if UI_TERMINAL
pj_ssize_t ui_input_terminal(char* const buf, pj_size_t len)
{
	if(fgets(buf, (int)len, stdin) != NULL) {
		return strlen(buf);
	}
	else {
		return -1;
	}
}

#if 0
pj_status_t ui_outp_terminal(const char* const msg, pj_ssize_t len) {
	printf(msg);
	return PJ_SUCCESS;
}
#endif
#endif





pj_status_t start_ssap_iface(void) {
  return update_ssap_iface();
}

pj_status_t update_ssap_iface(void) {
  pj_status_t res = PJ_SUCCESS;

  switch(conn_state) {
    case CONN_STATE_NOT_INITIALIZED: 
      listening_port = DEFAULT_DP_PORT;
      host_socket = 0l;
      client_socket = 0l;
      conn_state = CONN_STATE_NOT_CONNECTED;
      /* fall through */
    case CONN_STATE_NOT_CONNECTED:
      /* Setup socket. */

      res = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, &host_socket);
      if( res != 0 ) {
        PJ_PERROR(1, (THIS_FILE, res, "Creating socket descriptor."));
        break;
      }

      server_addr.sin_family = PJ_AF_INET;
      server_addr.sin_addr.s_addr = PJ_INADDR_ANY;
      server_addr.sin_port = pj_htons(listening_port);

      // bind
      res = pj_sock_bind(host_socket, &server_addr, sizeof(server_addr));
      if( res != 0 ) {
        PJ_PERROR(1, (THIS_FILE, res, "Binding socket."));
        break;
      }

      conn_state = CONN_STATE_LISTENING;
      /* else: fall through */
    case CONN_STATE_LISTENING:
      /* Setup listening. */
      res = pj_sock_listen( host_socket, 3 );
      if( res != PJ_SUCCESS ) {
        PJ_PERROR(2, (THIS_FILE, res, "Listening for connections"));
        break;
      }

      conn_state = CONN_STATE_WAITING;
      /* else: fall through */
    case CONN_STATE_WAITING:
      /* Wait for client connections */
      {
        int c = sizeof( pj_sockaddr_in );
        pj_sockaddr_in client_addr;
        pj_str_t msg_buffer;
      
        res = pj_sock_accept( host_socket, &client_socket, &client_addr, &c );
        if( res == PJ_SUCCESS ) {
          pj_in_addr client_ip = pj_sockaddr_in_get_addr(&client_addr);

          PJ_LOG(5, (THIS_FILE, "Accepting connection from %s", pj_inet_ntoa(client_ip)));
        } 
        else {
          PJ_PERROR(3, (THIS_FILE, res, "Accepting connection"));
          break;
        }
      }

      conn_state = CONN_STATE_CONNECTED;
      /* else: fall through */
    case CONN_STATE_CONNECTED:
      /* Connection established. Do nothing. */
      break;
    case CONN_STATE_ERROR:    /* fall through */
      res = -1;
    default:
      PJ_LOG(3, (THIS_FILE, "Connection state is invalid. Resetting."));
      break;
  }
}

pj_status_t close_ssap_connection(void) {
  if( client_socket != 0l ) {
    pj_status_t res = pj_sock_shutdown(client_socket, PJ_SHUT_RDWR);
    /* If closing the socket returns NOT_CONNECTED it has alreday been done.
     * Ignore that error. */
    if(!(res == PJ_SUCCESS || PJ_STATUS_TO_OS(res) == OSERR_ENOTCONN)) {
      PJ_PERROR(3, (THIS_FILE, res, "Closing client socket operations (0x%X)/(0x%X)", res, PJ_STATUS_TO_OS(res)));

      return res;
    }

    res = pj_sock_close(client_socket);
    if( res != PJ_SUCCESS ) {
      PJ_PERROR(3, (THIS_FILE, res, "Closing client socket descriptor"));
      return res;
    }

    client_socket = 0l;
    conn_state = CONN_STATE_WAITING;
  }

  return PJ_SUCCESS;
}

pj_status_t reset_ssap_connection(void) {
  pj_status_t res = close_ssap_connection();
  if(res == PJ_SUCCESS) {
    PJ_LOG(5, (THIS_FILE, "Restarting server."));
    res = update_ssap_iface();
  }
  return res;
}

pj_status_t reset_ssap_iface(void) {

  pj_status_t res = close_ssap_connection();
  if( res != PJ_SUCCESS ) {
    /* Closing client connection failed. Ignore this and attempt to recover anyway. */
    client_socket = 0l;
  }

  /* Close host socket. */
  res = teardown_ssap_iface();

  if(res == PJ_SUCCESS) {
    PJ_LOG(5, (THIS_FILE, "Restarting server."));
    res = update_ssap_iface();
  }
  else {
    PJ_LOG(2, (THIS_FILE, "Unrecoverable error during server reset. Will not restart."));
  }

  return res;
}

pj_status_t teardown_ssap_iface(void) {
  pj_status_t res = pj_sock_shutdown(host_socket, PJ_SHUT_RDWR);
  if(res != PJ_SUCCESS) {
    PJ_PERROR(3, (THIS_FILE, res, "Closing host socket operations"));
  }

  /* Attempt to close socket even if closing socket operations failed. */
  res = pj_sock_close(host_socket);
  if(res != PJ_SUCCESS) {
    PJ_PERROR(2, (THIS_FILE, res, "Closing host socket"));
  }

  conn_state = CONN_STATE_NOT_CONNECTED;

  return res;
}

pj_status_t dp_receive(char* const inp, pj_ssize_t lim) {
  pj_status_t res = pj_sock_recv(client_socket, inp, &lim, 0);

  switch(res) {
    case PJ_SUCCESS:
      inp[lim] = '\0';
      PJ_LOG(5, (THIS_FILE, "recv (%d): |%s|", strlen(inp), inp));
      break;
    case PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK):
      /* fall through */
    case PJ_STATUS_FROM_OS(OSERR_EINPROGRESS):
      PJ_PERROR(3, (THIS_FILE, res, "  %s ", __func__));
      break;
    case PJ_STATUS_FROM_OS(OSERR_ECONNRESET):
      // Connection reset by peer. Reset and listen for new connection.
      /* fall through. */
    case PJ_STATUS_FROM_OS(OSERR_ENOTCONN):
      PJ_PERROR(3, (THIS_FILE, res, "  %s ", __func__));
      reset_ssap_connection();
      break;
    case PJ_STATUS_FROM_OS(OSERR_EAFNOSUPPORT):
    case PJ_STATUS_FROM_OS(OSERR_ENOPROTOOPT):  
      PJ_PERROR(2, (THIS_FILE, res, "  %s ", __func__));
      break;
    case PJ_STATUS_FROM_OS(ENOTSOCK):
      PJ_PERROR(1, (THIS_FILE, res, "  %s ", __func__));
      /* Signal for program to quit. */
      strcpy(inp, "q\n");
      break;
    default:
      PJ_PERROR(2, (THIS_FILE, res, "Unhandled error pj(0x%X) os(%d)", res, PJ_STATUS_TO_OS(res)));
      break;
  }

  return res;
}

int dp_send(const void* const data, pj_ssize_t len) {
  PJ_LOG(5, (THIS_FILE, "send: |%s|", (char*) data));
  pj_status_t res = pj_sock_send(client_socket, data, &len, 0);
  if( res != PJ_SUCCESS ) {
    if(res == PJ_STATUS_FROM_OS(EPIPE)) {
      PJ_PERROR(3, (THIS_FILE, res, "Pipe error (0x%X).", res));
      /* Connection has been dropped. Reset server. */
      reset_ssap_connection();
    }
    else {
      PJ_PERROR(3, (THIS_FILE, res, "Error sending data to host (0x%X).", res));
    }
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
      PJ_LOG(2, (THIS_FILE, "Sending IM message: %s", inp+1));
      send_scaip_im(pj_strbuf(&arg));
      break;

    case 'm':
      PJ_LOG(2, (THIS_FILE, "Calling: %s", inp+1));
      make_scaip_call(pj_strbuf(&arg));
      break;

    case '#':
      {
        const int i = pjsua_call_get_count();
        //char buffer[8];
        PJ_LOG(2, (THIS_FILE, "Number of calls: %d", i));
        //sprintf(buffer, "%d", i);
        //dp_send(buffer, strlen(buffer));
        //printf("%d", i);
        data_print("%d", i);
      }
      break;

    case '\0':
      /*Empty command. Just ignore.*/
      break;
    default:
      PJ_LOG(5, (THIS_FILE, "Unknown scaip command: %c", c));
      break;
  }
}


void ui_scaip_keystroke_help(void) {
  const char help_text[] = "\n"
    "+=============================================================================+\n" 
    "|       SCAIP commands:                                                       |\n" 
    "|                                                                             |\n" 
    "| !h                This help.                                                |\n" 
    "| !i <addr> <msg>   Send SCAIP message. msg must be xml and message is sent   |\n" 
    "|                   with mime type application/scaip+xml.                     |\n" 
    "| !m <addr>         Call SIP addr.                                            |\n" 
    "| !#                Return number of active calls.                            |\n"
    "| !?                Return active call list.                                  |\n"
    "| !??               Return call quality.                                      |\n"
    "+=============================================================================+\n";

  //printf("%s", help_text);
  //dp_send(help_text, strlen(help_text));
  printf("%s", help_text);
}



static pj_status_t send_scaip_im(const char* const scaip_msg) {
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
  if((res = pjsua_verify_url(sip_address_buffer)) != PJ_SUCCESS ) {
    PJ_PERROR(3, (THIS_FILE, res, "Invalid sip address"));
    return res;
  }
  pj_strset2( &sip_address, sip_address_buffer );

  PJ_LOG(5, (THIS_FILE, "sip address: %s", sip_address));
  PJ_LOG(5, (THIS_FILE, "sip message: %s", sip_msg));

	pjsua_im_send(current_acc, &sip_address, &mime, &sip_msg, NULL, NULL);

  return PJ_SUCCESS;
}

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

