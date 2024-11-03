#ifndef _COMMS_H_
#define _COMMS_H_
// this exists in protobuf header this is not needed
#include "../include/entry.h"
#include "../protobuf/chord.pb-c.h"
ChordMessage *construct_msg(ChordMessage__MsgCase cs, int id, Entry *optional);
void send_message(ChordMessage *chord_msg, int dest_sock);
#endif
