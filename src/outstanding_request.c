#include "../include/outstanding_request.h"
#include "../include/time_utils.h"
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
  out_req->sent_message = sent_msg;
  if (sent_msg) {
    out_req->sent_message =
        (struct ChordMessage *)malloc(chord_message__get_packed_size(sent_msg));
    memcpy(out_req->sent_message, sent_msg,
           chord_message__get_packed_size(sent_msg));
  }
  if (dest_addr) {
    out_req->dest_addr = (struct sockaddr *)malloc(sizeof(struct sockaddr));
    memcpy(out_req->dest_addr, dest_addr, sizeof(struct sockaddr));
  }
  get_timeval_now(out_req->start_time);
  out_req->nth_transmission = 1;
  return out_req;
}

OutstandingRequest *construct_outstanding_request_with_id(
    int query_id, Msg_Type message_type, int finger_index, int id, int node_id,
    ChordMessage *sent_msg, struct sockaddr *dest_addr, int nth) {
  OutstandingRequest *out_req = construct_outstanding_request(
      query_id, message_type, finger_index, sent_msg, dest_addr, nth);
  out_req->input_id = id;
  out_req->node_id = node_id;
  return out_req;
}

void free_outstanding_request(OutstandingRequest *req) {
  free(req->sent_message);
  free(req->dest_addr);
  free(req);
}
