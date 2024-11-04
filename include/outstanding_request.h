#ifndef _OUTSTANDING_REQUESTH_
#define _OUTSTANDING_REQUESTH_
#include "../protobuf/chord.pb-c.h"

enum Msg_Type {
  join_find_successor,
  fix_fingers_find_successor,
  local_fix_fingers_find_successor,
  local_join_find_successor

};

typedef struct OutstandingRequest {
  OutstandingRequest *next;
  OutstandingRequest *prev;
  int query_id;
  Msg_Type message_type;
  int finger_index; // Only when message_type is fix_finger_request
} OutstandingRequest;

OutstandingRequest *construct_outstanding_request(int query_id,
                                                  Msg_Type message_type,
                                                  int finger_index);

#endif
