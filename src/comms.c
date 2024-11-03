#ifndef _COMMS_C_
#define _COMMS_C_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "comms.h"
#include "chord.h"
#include "entry.h"

//Sends the given ChordMessage to the given socket
void send_message(ChordMessage *chord_msg, int sock, struct sockaddr *info) {
    //only used to determine size of the 'len' field
    Message msg;

    size_t chord_msg_len = chord_message__get_packed_size(chord_msg);
    size_t buf_size = sizeof(msg.len) + chord_msg_len;
    void *msg_buf = malloc(buf_size);

    //I'm setting msg.len = chord_msg_len (so excludes the size of the len field itself)
    memcpy(msg_buf, chord_msg_len, sizeof(msg.len));
    chord_message__pack(chord_msg, msg_buf + sizeof(msg.len));
    sendto(sock, msg_buf, buf_size, 0, info, sizeof(struct sockaddr_in));
    free(msg_buf);
}

//Receives data from the given dest_sock and returns a ChordMessage with the received data.
//It also saves the sockaddr info of the message sender in info
ChordMessage *recv_message(int sock, struct sockaddr *info) {
    Message msg;
    recvfrom(sock, &msg.len, sizeof(msg.len), 0, info, sizeof(struct sockaddr_in));
    msg.ChordMessage = malloc(msg.len);

    recvfrom(sock, msg.ChordMessage, msg.len, 0, info, sizeof(struct sockaddr_in));
    ChordMessage *recvd_chord_msg = chord_message__unpack(NULL, msg.len, msg.ChordMessage);

    free(msg.ChordMessage);
    return recvd_chord_msg;
}

ChordMessage *construct_msg(ChordMessage__MsgCase cs, int id, Entry *optional) {
  ChordMessage *ret = 0;
  switch (cs) {

  case CHORD_MESSAGE__MSG_FIND_SUCCESSOR_REQUEST:

    ChordMessage cloak;
    FindSuccessorRequest rq;
    find_successor_request__init(&rq);
    chord_message__init(&cloak);
    cloak.version = 417;

    cloak.msg_case = CHORD_MESSAGE__MSG_FIND_SUCCESSOR_REQUEST;
    rq.key = id;
    cloak.find_successor_request = &rq;
    ret = &cloak;
    break;
  case CHORD_MESSAGE__MSG_NOTIFY_REQUEST:
    break;
  case CHORD_MESSAGE__MSG_NOTIFY_RESPONSE:
    break;
  case CHORD_MESSAGE__MSG_FIND_SUCCESSOR_RESPONSE:
    break;
  case CHORD_MESSAGE__MSG_GET_PREDECESSOR_REQUEST:
    break;
  case CHORD_MESSAGE__MSG_GET_PREDECESSOR_RESPONSE:
    break;
  case CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_REQUEST:
    break;
  case CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_RESPONSE:
    break;
  case CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_REQUEST:
    break;
  case CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_RESPONSE:
    break;
  }
  return ret;
}

#endif