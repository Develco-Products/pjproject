#include <pjsua-lib/pjsua.h>
#include "pjsua_app_common.h"

#include "pjsua_ssapmsg.h"
#include "pjlib.h"
#include "pjsua_app.h"
#include <string.h>
#include "pj/types.h"
#include "pjlib-util/crc32.h"
#include "pjsua_ssapcomm.h"

#define THIS_FILE "pjsua_ssapmsg"

/* keeps track of last sent message. 
 * Keep this static to initialize to 0. */
static struct {
  uint32_t ref;
  enum ssapmsg_type type;
} response_token;

static union ssapmsg_payload buffer;
static union ssapmsg_payload* shared_buffer = NULL;

static void sync_response(struct ssapmsg_datagram* const datagram);


char* ssapmsgtype_str(enum ssapmsg_type type) {
  char* ret = NULL;

  switch(type) {
    case SSAPMSG_UNDEFINED:
      ret = "SSAPMSG_UNDEFINED";
      break;
    case SSAPMSG_CALL:
      ret = "SSAPMSG_CALL";
      break;
    case SSAPMSG_SCAIPMSG:
      ret = "SSAPMSG_SCAIPMSG";
      break;
    case SSAPMSG_CONFIG:
      ret = "SSAPMSG_CONFIG";
      break;
    case SSAPMSG_PJSUA:
      ret = "SSAPMSG_PJSUA";
      break;
    /* SSAPMSG_NO_OF_MSG_TYPES: Invalid as message type. */
    case SSAPMSG_ACK:
      ret = "SSAPMSG_ACK";
      break;
    case SSAPMSG_NACK:
      ret = "SSAPMSG_NACK";
      break;
    case SSAPMSG_WARNING:
      ret = "SSAPMSG_WARNING";
      break;
    case SSAPMSG_ERROR:
      ret = "SSAPMSG_ERROR";
      break;
    default:
      ret = "INVALID SSAPMSG enum value.";
      break;
  }

  return ret;
}

pj_status_t ssapmsg_parse(struct ssapmsg_iface* dst, uint8_t* msg, pj_ssize_t msg_size) {
  pj_status_t res = PJ_SUCCESS;

  //PJ_LOG(3, (THIS_FILE, "sizeof enum: %d", sizeof(enum ssapmsg_type)));

  if(msg_size < SSAPMSG_HEADER_SIZE) {
    PJ_LOG(3, (THIS_FILE, "Message min. size check: %d < %d", msg_size, sizeof(struct ssapmsg_datagram)));
    return PJ_ETOOSMALL;
  }

  if(msg_size >= sizeof(struct ssapmsg_datagram)) {
    PJ_LOG(3, (THIS_FILE, "Message size %dB exceeds buffer size of %dB", msg_size, sizeof(struct ssapmsg_datagram)));
    return PJ_ETOOBIG;
  }

  struct ssapmsg_datagram* datagram = (struct ssapmsg_datagram*) msg;

  //TODO: Add crc check.
  PJ_LOG(3, (THIS_FILE, "TODO: Add CRC check of ssap package."));
  
  //pj_memcpy((void*)dst->raw, (void*)&hdr->payload, hdr->payload_size);
  dst->ref = datagram->ref;
  dst->type = datagram->type;

  /* Add any special handling of specific payload types. */
  switch(datagram->type) {
    case SSAPMSG_UNDEFINED:
      //break;
    case SSAPMSG_CALL:
      PJ_LOG(3, (THIS_FILE, "Parsing not implemented for message type: %s", datagram->type));
      break;
    case SSAPMSG_SCAIPMSG:
      /* Initialize address and message pointers to correct points in the data buffer. */
      if(datagram->payload.scaipmsg.msg_offset >= datagram->payload_size) {
        PJ_LOG(3, (THIS_FILE, "Contents of scaip message corrupted."));
        res = PJ_EINVAL;
      }
      else {
        dst->address = datagram->payload.scaipmsg.data;
        dst->msg = datagram->payload.scaipmsg.data + datagram->payload.scaipmsg.msg_offset;
      }
      break;
    case SSAPMSG_CONFIG:
      PJ_LOG(3, (THIS_FILE, "Parsing not implemented for message type: %s", datagram->type));
      break;
    case SSAPMSG_PJSUA:
      // pjsua commands are textbased, so ensure null-termination.
      dst->data = datagram->payload.pjsua.data;
      //dst->raw[hdr->payload_size] = '\0';
      break;
    case SSAPMSG_ACK:
      //break;
    case SSAPMSG_NACK:
      //break;
    case SSAPMSG_WARNING:
      //break;
    case SSAPMSG_ERROR:
      PJ_LOG(3, (THIS_FILE, "Parsing not implemented for message type: %s", datagram->type));
      break;
    defualt:
      PJ_LOG(3, (THIS_FILE, "Invalid payload type (%d) in header.", datagram->type));
      break;
  }

  return PJ_SUCCESS;
}


void ssapmsg_print(const struct ssapmsg_datagram* const msg) {
  char buffer[1024];

  PJ_LOG(3, (THIS_FILE, "SSAP Message -------------"));
  PJ_LOG(3, (THIS_FILE, "Header"));
  PJ_LOG(3, (THIS_FILE, "  Ref:           %d / 0x%X", msg->ref, msg->ref));
  switch(msg->type) {
    case SSAPMSG_UNDEFINED:
      PJ_LOG(3, (THIS_FILE, "  Type:         Undefined (0x%x)", msg->type));
      strcpy(buffer, " - no data -");
      break;
    case SSAPMSG_CALL:
      PJ_LOG(3, (THIS_FILE, "  Type:         Call (0x%x)", msg->type));
      strcpy(buffer, " - not implemented -");
      break;
    case SSAPMSG_SCAIPMSG:
      PJ_LOG(3, (THIS_FILE, "  Type:         Scaip Message (0x%x)", msg->type));
      sprintf(buffer,       "  offset:       %d\n"
                            "  data:         %s",
                            msg->payload.scaipmsg.msg_offset,
                            (char*) msg->payload.scaipmsg.data);
      break;
    case SSAPMSG_CONFIG:
      PJ_LOG(3, (THIS_FILE, "  Type:         Config (0x%x)", msg->type));
      strcpy(buffer, " - not implemented -");
      break;
    case SSAPMSG_PJSUA:
      PJ_LOG(3, (THIS_FILE, "  Type:          Pjsua (0x%x)", msg->type));
      sprintf(buffer,       "  data:         %s", msg->payload.pjsua.data);
      break;
    case SSAPMSG_ACK:
      PJ_LOG(3, (THIS_FILE, "  Type:          Ack (0x%x)", msg->type));
      break;
    case SSAPMSG_NACK:
      PJ_LOG(3, (THIS_FILE, "  Type:          Nack (0x%x)", msg->type));
      break;
    case SSAPMSG_WARNING:
      PJ_LOG(3, (THIS_FILE, "  Type:         Warning (0x%x)", msg->type));
      strcpy(buffer, " - no data -");
      break;
    case SSAPMSG_ERROR:
      PJ_LOG(3, (THIS_FILE, "  Type:         Error (0x%x)", msg->type));
      strcpy(buffer, " - no data -");
      break;
    default:
      PJ_LOG(3, (THIS_FILE, "  Type:         - no type - (0x%x)", msg->type));
      strcpy(buffer, " - no data -");
      break;
  }
  PJ_LOG(3, (THIS_FILE, "  Payload size:  %dB / 0x%X", msg->payload_size, msg->payload_size));
  PJ_LOG(3, (THIS_FILE, "  CRC:           0x%X", msg->crc));
  PJ_LOG(3, (THIS_FILE, "Payload"));
  PJ_LOG(3, (THIS_FILE, "%s", buffer));
  PJ_LOG(3, (THIS_FILE, "-- End of message: %d", msg->ref));
}


void update_crc32(struct ssapmsg_datagram* const ssapmsg) {
  /* Clear out any existing crc before calculation. */
  ssapmsg->crc = 0u;

  uint32_t crc = pj_crc32_calc((uint8_t*) ssapmsg, ssapmsg->payload_size + SSAPMSG_HEADER_SIZE);
  ssapmsg->crc = crc;

  PJ_LOG(5, (THIS_FILE, "crc calculated: 0x%X", crc));
}


pj_status_t validate_ssapmsg_crc(struct ssapmsg_datagram* const ssapmsg) {
  uint32_t msg_crc = ssapmsg->crc;
  uint32_t msg_calc;

  /* Zero out crc for use in calculation. */
  ssapmsg->crc = 0u;

  /* Calculate crc for entire buffer. */
  msg_calc = pj_crc32_calc( (uint8_t*) ssapmsg, ssapmsg->payload_size + SSAPMSG_HEADER_SIZE );

  /* Restore message crc so the original message is restored. */
  ssapmsg->crc = msg_crc;

  PJ_LOG(5, (THIS_FILE, "crc validation. Received: 0x%X, Calculated: 0x%X", msg_crc, msg_calc));
  
  return (msg_calc == msg_crc)
    ? PJ_SUCCESS
    : PJ_EINVAL;
}


pj_status_t ssapmsg_receive(struct ssapmsg_datagram* const msg) {
  pj_ssize_t lim = sizeof(struct ssapmsg_datagram);
  pj_status_t res = ssapsock_receive((uint8_t*) msg, &lim);

  switch(res) {
    case PJ_SUCCESS:
      /* Validate that packet has valid size value. */
      if(msg->payload_size + SSAPMSG_HEADER_SIZE != lim) {
        PJ_LOG(3, (THIS_FILE, "Packet is malformed. Size: %dB, Received: %d", msg->payload_size + SSAPMSG_HEADER_SIZE, lim));
        return PJ_EINVAL;
      }

      /* validate packet crc. */
      if(validate_ssapmsg_crc(msg) != PJ_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "crc validation failed. Currently results in no action."));
      }

      /* Set token for response. */
      //struct response_token* const token = &response_token[msg->type];

      if(response_token.type != SSAPMSG_UNDEFINED) {
        PJ_LOG(3, (THIS_FILE, "Overwriting existing pending response token for: %s with id: %d", ssapmsgtype_str(response_token.type), response_token.ref));
      }

      response_token.ref  = msg->ref;
      response_token.type = msg->type;

      PJ_LOG(5, (THIS_FILE, "Setting pending response for %s with id: %d", ssapmsgtype_str(response_token.type), response_token.ref));
      break;

    case PJ_STATUS_FROM_OS(ENOTSOCK):
      {
        const char* const cmd_quit = "q\n";
        const uint32_t l = strlen(cmd_quit) +1;
        //struct ssapmsg_datagram msg = {
        //  .ref = 0,
        //  .type = SSAPMSG_PJSUA,
        //  .payload_size = l,
        //  .crc = 0
        //};
        //pj_memcpy((void*) &msg.payload.pjsua, cmd_quit, l);

        msg->ref = 0; // ref dummy value.
        msg->type = SSAPMSG_PJSUA;
        msg->payload_size = l;
        msg->crc = 0u;  // dummy crc.
        pj_memcpy(msg->payload.pjsua.data, cmd_quit, l);

        //lim = SSAPMSG_HEADER_SIZE + l;
        //pj_memcpy(inp, &msg, *lim);

        //PJ_PERROR(1, (THIS_FILE, res, "  %s ", __func__));
        /* Signal for program to quit. */
        //strcpy(inp, "q\n");
        res = PJ_SUCCESS;
      }
      break;

    default:
      /* No a result which is handled here. */
      break;
  }

  return res;
}


pj_ssize_t ssapmsg_send(struct ssapmsg_datagram* const msg) {
  /* Add crc to packet. */
  update_crc32(msg);

  pj_ssize_t msg_size = msg->payload_size + SSAPMSG_HEADER_SIZE;
  const pj_ssize_t expected_transmission_length = msg_size;
  pj_status_t res = ssapsock_send((void*) msg, &msg_size);

  /* Error handling already exists at socket layer.
   * Check for size mismatch for succussful transmissions. */
  if( res == PJ_SUCCESS ) {
    if( msg_size != expected_transmission_length ) {
      PJ_LOG(3, (THIS_FILE, "Failed sending expected number of bytes. Expected: %d, sent: %d", expected_transmission_length, msg_size));
      res = PJ_EBUG;
    }

    /* Message transmission successful. Clear pending response. */
    response_token.ref = 0u;
    response_token.type = SSAPMSG_UNDEFINED;
  }

  return res;
}

pj_status_t ssapmsg_send_response(const pj_status_t resp, const char* const msg) {
  struct ssapmsg_datagram datagram;
  sync_response(&datagram);

  datagram.type = (resp == PJ_SUCCESS)
    ? SSAPMSG_ACK
    : SSAPMSG_NACK;

  // Set payload
  datagram.payload.response.code = resp;

  if( msg != NULL ) {
    pj_ssize_t msg_length = strlen(msg) +1;
    datagram.payload_size = sizeof(struct ssapmsg_response) + msg_length;

    pj_memcpy(datagram.payload.response.msg, msg, msg_length);
  }
  else {
    datagram.payload_size = sizeof(struct ssapmsg_response) + 1;
    datagram.payload.response.msg[0] = '\0';
  }
  
  pj_ssize_t b_sent = ssapmsg_send(&datagram);

  return (b_sent == SSAPMSG_HEADER_SIZE)
    ? PJ_SUCCESS
    : PJ_EUNKNOWN;
}

pj_ssize_t ssapmsg_scaipmsg_send(const char* const addr, const char* const msg) {
  struct ssapmsg_datagram datagram;
  const ssize_t addr_length = strlen(addr) +1;
  const ssize_t msg_length = strlen(msg) +1;

  sync_response(&datagram);

  datagram.type = SSAPMSG_SCAIPMSG;
  datagram.payload_size = sizeof(struct ssapmsg_scaipmsg) + addr_length + msg_length;

  /* Pack payload. */
  struct ssapmsg_scaipmsg* p = &datagram.payload.scaipmsg;
  pj_memcpy(p->data, addr, addr_length);
  p->msg_offset = addr_length +1;
  pj_memcpy(p->data + p->msg_offset, msg, msg_length);

  /* Send it. */
  ssapmsg_send(&datagram);
}

static void sync_response(struct ssapmsg_datagram* const datagram) {
  if(response_token.type == SSAPMSG_UNDEFINED) {
    PJ_LOG(3, (THIS_FILE, "Message flow is out of sync! No response is pending."));
    datagram->ref = -1u;
  }
  else {
    datagram->ref = response_token.ref;
  }
}

