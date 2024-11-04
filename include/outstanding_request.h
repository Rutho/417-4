#include "chord.pb-c.h"

typedef struct OutstandingRequest {
  OutstandingRequest *next;
  OutstandingRequest *prev;
  int query_id;
  ChordMessage__MsgCase message_type;
  int finger_index; //Only when message_type is fix_finger_request
} OutstandingRequest;

OutstandingRequest *construct_outstanding_request(int query_id, ChordMessage__MsgCase message_type, int finger_index);