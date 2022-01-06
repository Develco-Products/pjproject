
#include <pjsua-lib/pjsua.h>
#include "pjsua_app_common.h"

#include "pjsua_ssapcomm.h"
#include "pjlib.h"
#include "pjsua_app.h"
#include <string.h>
#include "pj/types.h"
//#include "pjsua_ssapmsg.h"
#include "pjsua_app_scaip.h"
#include "pjsua_ssaperr.h"

#define THIS_FILE "pjsua_ssapcomm"

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

#define TRANSMISSION_QUEUE_SIZE 1024

// global
// ssap_mem - global memory pool for pjsua-ssap.
// located in pjsua_app_legacy.c
extern pj_caching_pool ssap_mem;

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
  CONN_STATE_START_TRANSMITTER,
  CONN_STATE_CONNECTED,
  CONN_STATE_DISCONNECTED,
  CONN_STATE_ERROR
} conn_state = CONN_STATE_NOT_INITIALIZED;

struct transmission_queue_node {
  PJ_DECL_LIST_MEMBER(struct transmission_queue_node);
  pj_ssize_t len;
  uint8_t data[0];
};

static pj_pool_t* rcv_pool = NULL;


static pj_pool_t* pool = NULL;
static pj_thread_t* transmitter = NULL;
static pj_pool_t* queue_pool = NULL;
static struct transmission_queue_node queue_head = { .len = 0 };
static pj_mutex_t* transmitter_mut = NULL;

static pj_status_t last_transmission_status = PJ_SUCCESS;

static pj_status_t stop_transmitter();
static int ssapsock_transmitter(void);


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
      pj_list_init(&queue_head);

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
        // TODO: Add check for allowing remote connections.

        int c = sizeof( pj_sockaddr_in );
        pj_sockaddr_in client_addr;
        pj_str_t msg_buffer;
      
        PJ_LOG(3, (THIS_FILE, "Waiting for incoming connection..."));
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

      conn_state = CONN_STATE_START_TRANSMITTER;
      /* else: fall through */
    case CONN_STATE_START_TRANSMITTER:
      if(pool != NULL || transmitter != NULL || transmitter_mut != NULL) {
        PJ_PERROR(3, (THIS_FILE, SSAP_EMEMLEAK, "Possible memory leak detected."));
      }

      pj_list_init(&queue_head);

      const pj_ssize_t pool_size = PJ_THREAD_DESC_SIZE
          + 64 // est. mutex size.
          + TRANSMISSION_QUEUE_SIZE;
      pool = pj_pool_create(&ssap_mem.factory, "ssap_socket", pool_size, 32, NULL);
      if(pool == NULL) {
        PJ_PERROR(1, (THIS_FILE, PJ_ENOMEM, "Not possible to allocate memory for socket communication."));
        res = PJ_ENOMEM;
        conn_state = CONN_STATE_ERROR;
        break;
      }

      res = pj_mutex_create(pool, "transmitter_queue_lock", PJ_MUTEX_SIMPLE, &transmitter_mut);
      if(res != PJ_SUCCESS) {
        PJ_PERROR(3, (THIS_FILE, res, "Could not create queue mutex."));
        conn_state = CONN_STATE_ERROR;
        break;
      }

      res = pj_thread_create(pool, "transmitter", (pj_thread_proc*)ssapsock_transmitter, NULL, PJ_THREAD_DEFAULT_STACK_SIZE, 0, &transmitter);
      if(res != PJ_SUCCESS) {
        PJ_PERROR(3, (THIS_FILE, res, "Could not create transmitter thread."));
        conn_state = CONN_STATE_ERROR;
        break;
      }

      conn_state = CONN_STATE_CONNECTED;
      /* else: fall through */
    case CONN_STATE_CONNECTED:
      /* Connection established. Do nothing. */
      break;
    case CONN_STATE_DISCONNECTED:
      /* Wait state while connection is shutting down. */
      break;
    case CONN_STATE_ERROR:    /* fall through */
      res = -1;
      break;
    default:
      PJ_LOG(3, (THIS_FILE, "Connection state is invalid."));
      break;
  }
}

pj_status_t close_ssap_connection(void) {
  if( client_socket != 0l ) {
    stop_transmitter();

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
    pj_thread_sleep(3000);
    PJ_LOG(5, (THIS_FILE, "Restarting server."));
    res = update_ssap_iface();
  }
  else {
    PJ_LOG(2, (THIS_FILE, "Unrecoverable error during server reset. Will not restart."));
  }

  return res;
}

pj_status_t teardown_ssap_iface(void) {
  stop_transmitter();

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

pj_bool_t ssap_connection_up(void) {
  return conn_state == CONN_STATE_CONNECTED;
}

pj_status_t ssapsock_receive(uint8_t* const inp, pj_ssize_t* lim) {
  pj_status_t res = pj_sock_recv(client_socket, inp, lim, 0);
  int i;

  switch(res) {
    case PJ_SUCCESS:
      inp[*lim] = '\0';
      PJ_LOG(3, (THIS_FILE, "received %d bytes", *lim));
#if 0
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
#endif
      print_raw_hex(inp, *lim);
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
    //case PJ_STATUS_FROM_OS(9):
    case PJ_STATUS_FROM_OS(EBADF):
      PJ_LOG(2, (THIS_FILE, "Well, if you are not here, I don't know where you are."));
      PJ_PERROR(3, (THIS_FILE, res, "  %s ", __func__));
      reset_ssap_connection();
      break;
    case PJ_STATUS_FROM_OS(OSERR_EAFNOSUPPORT):
    case PJ_STATUS_FROM_OS(OSERR_ENOPROTOOPT):  
    case PJ_STATUS_FROM_OS(ENOTSOCK):
    case PJ_STATUS_FROM_OS(EFAULT):
      /* Handled at message layer. */
#if 0
      {
        PJ_PERROR(2, (THIS_FILE, res, "  %s ", __func__));

        const char* const quit_cmd = "q\n";
        ssapmsg_datagram_t msg = { 
          .ref = 0u,
          .type = SSAPMSG_PJSUA,
          .payload_size = strlen(quit_cmd) +1
        };
        pj_memcpy(msg.payload.raw, quit_cmd, strlen(quit_cmd) +1);
        update_crc32(&msg);

        pj_memcpy((void*) inp, (void*) &msg, msg.payload_size + SSAPMSG_HEADER_SIZE);
        res = PJ_SUCCESS;
      }
#endif
      PJ_PERROR(2, (THIS_FILE, res, "  %s ", __func__));
      break;
    default:
      PJ_PERROR(2, (THIS_FILE, res, "Unhandled error pj(0x%X) os(%d)", res, PJ_STATUS_TO_OS(res)));
      break;
  }

  return res;
}

pj_status_t ssapsock_transmission_status() {
  pj_status_t status;

  do {
    status = last_transmission_status;
  } while(status != last_transmission_status);

  return status;
}

//pj_status_t ssapsock_queue(const void* const data, pj_ssize_t len) {
pj_status_t ssapsock_send(const void* const data, pj_ssize_t* len) {
  if(len > 0) {
    /* Allocate new node and add the data to it. */
    pj_mutex_lock(transmitter_mut);

    if(queue_pool == NULL) {
      PJ_LOG(3, (THIS_FILE, "Allocating memory for message queue."));
      queue_pool = pj_pool_create(&ssap_mem.factory, "transmission_queue", TRANSMISSION_QUEUE_SIZE, TRANSMISSION_QUEUE_SIZE /2, NULL);
      if(queue_pool == NULL) {
        PJ_PERROR(3, (THIS_FILE, PJ_ENOMEM, "Could not allocate memory for transmission queue"));
        return PJ_ENOMEM;
      }
    }

    PJ_LOG(3, (THIS_FILE, "Allocating %dB of memory for message.", *len));
    struct transmission_queue_node* node = pj_pool_alloc(queue_pool, sizeof(struct transmission_queue_node) + *len);
    if(node == NULL) {
      PJ_PERROR(3, (THIS_FILE, PJ_ENOMEM, "Could not allocate memory for message"));
      return PJ_ENOMEM;
    }
    pj_memcpy(node->data, data, *len);
    node->len = *len;
    pj_list_push_back(&queue_head, node);
    pj_mutex_unlock(transmitter_mut);
  }

  return PJ_SUCCESS;
}


void ssapsock_send_blind(const void* const data, pj_ssize_t len) {
  (void) ssapsock_send(data, &len);
}


int ssapsock_transmitter() {
  while(conn_state != CONN_STATE_DISCONNECTED) {
    struct transmission_queue_node* node = NULL;

    pj_mutex_lock(transmitter_mut);
    if(!pj_list_empty(&queue_head)) {
      node = pj_list_shift(&queue_head);
    }
    pj_mutex_unlock(transmitter_mut);

    if(node != NULL) {
      LOG_DATA_OUTPUT(node->data);

      pj_status_t res = pj_sock_send(client_socket, node->data, &node->len, 0);

      pj_mutex_lock(transmitter_mut);
      last_transmission_status = res;

      /* If queue is still empty at this point, release pool. */
      if(pj_list_empty(&queue_head)) {
        PJ_LOG(3, (THIS_FILE, "Releasing queue pool."));
        pj_pool_release(queue_pool);
        queue_pool = NULL;
      }
      pj_mutex_unlock(transmitter_mut);

      if(res != PJ_SUCCESS) {
        PJ_PERROR(3, (THIS_FILE, res, "Error during transmission (0x%X). Suspending transmission for 60s.", res));
        pj_thread_sleep(60000);
      }
    }
    else {
      pj_thread_sleep(500);
    }
  }
}


void print_raw_hex(const void* const data, pj_ssize_t len) {
	if(len < 0) { return; }

	// buffer size of 3B for each entry +null.
#define LINE_WIDTH 16
	char buffer[3 * LINE_WIDTH +1] = { '\0' };
	for(pj_ssize_t i = 0; i < len; i+=LINE_WIDTH) {
		char* p = buffer;
		for(int j=0; j<LINE_WIDTH && i+j < len; ++j) {
			sprintf(p, "%02X ", ((uint8_t*)data)[i+j]);
      p += 3;
		}
		PJ_LOG(3, (THIS_FILE, "%s", buffer));
	}
#undef LINE_WIDTH
}


static pj_status_t stop_transmitter() {
  /* Set state to disconnect to flag no more actions should be performed 
   * on socket while we are shutting down. */
  conn_state = CONN_STATE_DISCONNECTED;

  if(transmitter != NULL) {
    pj_thread_join(transmitter);
    transmitter = NULL;
  }

  if(transmitter_mut != NULL) {
    pj_mutex_destroy(transmitter_mut);
    transmitter_mut = NULL;
  }

  if(pj_list_size(&queue_head) != 0) {
    PJ_LOG(3, (THIS_FILE, "There are unsent messages in queue. Dropping them."));
    pj_list_init(&queue_head);    // re-init list removes all nodes.
  }

  if(queue_pool != NULL) {
    pj_pool_release(queue_pool);
    queue_pool = NULL;
  }

  if(pool != NULL) {
    //pjsua_release_pool(pool);
    pj_pool_release(pool);
    pool = NULL;
  }
}

