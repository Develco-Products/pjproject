
#include <pjsua-lib/pjsua.h>
#include "pjsua_app_common.h"

#include "pjsua_ssapcomm.h"
#include "pjlib.h"
#include "pjsua_app.h"
#include <string.h>
#include "pj/types.h"
//#include "pjsua_ssapmsg.h"
#include "pjsua_app_scaip.h"

#define THIS_FILE "pjsua_dp"

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
	pj_status_t res = ssapsock_receive(buf, &len);

	if(res == PJ_SUCCESS) {
		return len;
	}
	else {
		PJ_PERROR(3, (THIS_FILE, res, "  (%s) reading data from socket: ", __func__));
		return -1;
	}
}
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

pj_status_t ssapsock_receive(uint8_t* const inp, pj_ssize_t* lim) {
  pj_status_t res = pj_sock_recv(client_socket, inp, lim, 0);
  int i;

  switch(res) {
    case PJ_SUCCESS:
      inp[*lim] = '\0';
      PJ_LOG(3, (THIS_FILE, "received %d bytes", *lim));
      for(i=0; i<*lim; i+=16) {
        PJ_LOG(3, (THIS_FILE, "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
              (i +0 <*lim ? (uint8_t)inp[i +0]  : 255),
              (i +1 <*lim ? (uint8_t)inp[i +1]  : 255),
              (i +2 <*lim ? (uint8_t)inp[i +2]  : 255),
              (i +3 <*lim ? (uint8_t)inp[i +3]  : 255),
              (i +4 <*lim ? (uint8_t)inp[i +4]  : 255),
              (i +5 <*lim ? (uint8_t)inp[i +5]  : 255),
              (i +6 <*lim ? (uint8_t)inp[i +6]  : 255),
              (i +7 <*lim ? (uint8_t)inp[i +7]  : 255),
              (i +8 <*lim ? (uint8_t)inp[i +8]  : 255),
              (i +9 <*lim ? (uint8_t)inp[i +9]  : 255),
              (i +10<*lim ? (uint8_t)inp[i +10] : 255),
              (i +11<*lim ? (uint8_t)inp[i +11] : 255),
              (i +12<*lim ? (uint8_t)inp[i +12] : 255),
              (i +13<*lim ? (uint8_t)inp[i +13] : 255),
              (i +14<*lim ? (uint8_t)inp[i +14] : 255),
              (i +15<*lim ? (uint8_t)inp[i +15] : 255)
              ));
      }
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
    case PJ_STATUS_FROM_OS(ENOTSOCK):
      PJ_PERROR(2, (THIS_FILE, res, "  %s ", __func__));
      break;
    default:
      PJ_PERROR(2, (THIS_FILE, res, "Unhandled error pj(0x%X) os(%d)", res, PJ_STATUS_TO_OS(res)));
      break;
  }

  return res;
}

pj_status_t ssapsock_send(const void* const data, pj_ssize_t* len) {
  LOG_DATA_OUTPUT();

  pj_status_t res = pj_sock_send(client_socket, data, len, 0);
  if( res == PJ_SUCCESS ) {
    /* Set pending response token. */
    //ssapmsg_response_pending.ref
  }
  else {
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

/* Just throw something out there and not care how it went. */
void ssapsock_send_blind(const void* const data, pj_ssize_t len) {
  (void) ssapsock_send(data, &len);
}



