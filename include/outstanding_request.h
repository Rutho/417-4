#ifndef OUTSTANDING_REQUEST_H
#define OUTSTANDING_REQUEST_H

#include "sys/time.h"

#include "../protobuf/chord.pb-c.h"

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
  int input_id;
  int query_id;
  int node_id;
  Msg_Type message_type;
  int finger_index; // Only when message_type is fix_finger_find_successor
  struct timeval *start_time;
  int nth_transmission; // 1 if original transmission, +1 if retransmission
  ChordMessage *sent_message;
  struct sockaddr *dest_addr;
};

OutstandingRequest *construct_outstanding_request(int query_id,
                                                  Msg_Type message_type,
                                                  int finger_index,
                                                  ChordMessage *sent_msg,
                                                  struct sockaddr *dest_addr);

OutstandingRequest *construct_outstanding_request_with_id(
    int query_id, Msg_Type message_type, int finger_index, int id, int node_id,
    ChordMessage *sent_msg, struct sockaddr *dest_addr);

void free_outstanding_request(OutstandingRequest *req);

#endif
