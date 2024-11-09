#ifndef _OUTSTANDING_REQUESTH_
#define _OUTSTANDING_REQUESTH_

#include "sys/time.h"

#include "../protobuf/chord.pb-c.h"

typedef enum Msg_Type {
  join_find_successor,
  fix_fingers_find_successor,
  local_fix_fingers_find_successor,
  local_join_find_successor,
  stabilize_get_predecessor,
  user_lookup
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
};

OutstandingRequest *construct_outstanding_request(int query_id,
                                                  Msg_Type message_type,
                                                  int finger_index);

OutstandingRequest *construct_outstanding_request_with_id(int query_id,
                                                          Msg_Type message_type,
                                                          int finger_index,
                                                          int id, int node_id);

#endif
