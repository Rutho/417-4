#include "../include/outstanding_request.h"
#include "../include/comms.h"
#include "../include/time_utils.h"
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>

// finger_index only relevant for fix_finger request type. finger_index won't be
// read when handling any other message types
OutstandingRequest *
construct_outstanding_request(int query_id, Msg_Type message_type,
                              int finger_index, ChordMessage *sent_msg,
                              struct sockaddr *dest_addr, int nth) {
  OutstandingRequest *out_req =
      (OutstandingRequest *)malloc(sizeof(OutstandingRequest));
  out_req->query_id = query_id;
  out_req->message_type = message_type;
  out_req->finger_index = finger_index;
  out_req->next = out_req->prev = out_req;
  out_req->input_id = -1;
  out_req->node_id = -1;
  out_req->sent_message = NULL;
  out_req->dest_addr = NULL;

  if (sent_msg != NULL) {
    out_req->sent_message = sent_msg;
  }
  if (dest_addr != NULL) {
    
    struct sockaddr_in *demp = malloc(sizeof(struct sockaddr));
    struct sockaddr_in *memp = (struct sockaddr_in *)dest_addr;
    demp->sin_family = AF_INET;
    demp->sin_port = memp->sin_port;
    demp->sin_addr.s_addr = memp->sin_addr.s_addr;
    out_req->dest_addr = (struct sockaddr *)demp;
  }
  // not declared?
  get_timeval_now(&(out_req->start_time));
  // nth should be one

  out_req->nth_transmission = nth;
  return out_req;
}

OutstandingRequest *
construct_outstanding_request_with_id(int query_id, Msg_Type message_type,
                                      int finger_index, uint64_t id,
                                      uint64_t node_id, ChordMessage *sent_msg,
                                      struct sockaddr *dest_addr, int nth) {
  OutstandingRequest *out_req = construct_outstanding_request(
      query_id, message_type, finger_index, sent_msg, dest_addr, nth);
  out_req->input_id = id;
  out_req->node_id = node_id;

  return out_req;
}

void free_outstanding_request(OutstandingRequest *req) {
  if (req->sent_message != NULL) {

    destroy_msg(req->sent_message);
  }
  if (req->dest_addr != NULL) {

    free(req->dest_addr);
  }
  free(req);
}
