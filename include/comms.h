#ifndef COMMS_H
#define COMMS_H
// this exists in protobuf header this is not needed
#include "../include/entry.h"
#include "../protobuf/chord.pb-c.h"

ChordMessage *construct_msg(ChordMessage__MsgCase cs, int id, int qid,
                            Entry *optional);
int send_message(ChordMessage *chord_msg, int sock, struct sockaddr *info);
ChordMessage *recv_message(int sock, struct sockaddr *info);
int generate_query_id();
int destroy_msg(ChordMessage *msg);
#endif
