#ifndef _COMMS_H_
#define _COMMS_H_
// this exists in protobuf header this is not needed
#include "../include/entry.h"
#include "../protobuf/chord.pb-c.h"

typedef struct OutstandingRequest {
  OutstandingRequest *next;
  OutstandingRequest *prev;
  int query_id;
  ChordMessage__MsgCase message_type;
  int finger_index; //Only when message_type is fix_finger_request
} OutstandingRequest;

ChordMessage *construct_msg(ChordMessage__MsgCase cs, int id, Entry *optional);
int send_message(ChordMessage *chord_msg, int dest_sock);
ChordMessage *recv_message(int dest_sock);
OutstandingRequest *construct_outstanding_request(int query_id, ChordMessage__MsgCase message_type, int finger_index);
#endif
