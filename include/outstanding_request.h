#ifndef OUTSTANDING_REQUEST_H
#define OUTSTANDING_REQUEST_H

#include "sys/time.h"

#include "../protobuf/chord.pb-c.h"
#include "stdint.h"

typedef enum Msg_Type {
  join_find_successor,
  fix_fingers_find_successor,
  local_fix_fingers_find_successor,
  stabilize_get_predecessor,
  user_lookup,
  check_pred,
  notify_send,
  get_pred,
  stablize_get_successor_list
} Msg_Type;

typedef struct OutstandingRequest OutstandingRequest;
struct OutstandingRequest {
  OutstandingRequest *next;
  OutstandingRequest *prev;
  uint64_t input_id;
  int query_id;
  uint64_t node_id;
  Msg_Type message_type;
  int finger_index; // Only when message_type is fix_finger_find_successor
  struct timeval start_time;
  int nth_transmission; // 1 if original transmission, >1 if retransmission
  ChordMessage *sent_message;
  struct sockaddr *dest_addr;
  int nth_closest_predecessor;
};

OutstandingRequest *
construct_outstanding_request(int query_id, Msg_Type message_type,
                              int finger_index, ChordMessage *sent_msg,
                              struct sockaddr *dest_addr, int nth);

OutstandingRequest *construct_outstanding_request_with_id(
    int query_id, Msg_Type message_type, int finger_index, uint64_t id,
    uint64_t node_id, ChordMessage *sent_msg, struct sockaddr *dest_addr,
    int nth_closest_predecessor);

void free_outstanding_request(OutstandingRequest *req);

#endif
