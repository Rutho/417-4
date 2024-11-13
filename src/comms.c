#ifndef _COMMS_C_
#define _COMMS_C_

#include <math.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include <endian.h>
#include <math.h>

#include "../include/chord.h"
#include "../include/comms.h"
#include "../include/entry.h"
#include "../include/entry_list.h"

int generate_query_id() { return rand(); }

// Sends the given ChordMessage to the given socket
int send_message(ChordMessage *chord_msg, int sock, struct sockaddr *info) {
  // only used to determine size of the 'len' field
  Message msg;

  size_t chord_msg_len = chord_message__get_packed_size(chord_msg);
  size_t buf_size = sizeof(msg.len) + chord_msg_len;
  void *msg_buf = malloc(buf_size);

  // I'm settstruct sockaddr_in address;ing msg.len = chord_msg_len (so excludes
  // the size of the len field itself)
  memcpy(msg_buf, &chord_msg_len, sizeof(msg.len));
  chord_message__pack(chord_msg, msg_buf + sizeof(msg.len));
  sendto(sock, msg_buf, buf_size, 0, info, sizeof(struct sockaddr_in));
  free(msg_buf);
  return chord_msg->query_id;
}

// Receives data from the given dest_sock and returns a ChordMessage with the
// received data. It also saves the sockaddr info of the message sender in info
ChordMessage *recv_message(int sock, struct sockaddr *info) {
  Message msg;

  socklen_t len = sizeof(struct sockaddr_in);

  recvfrom(sock, &msg.len, sizeof(msg.len), MSG_PEEK, info, &len);

  msg.ChordMessage = malloc(msg.len);

  recvfrom(sock, msg.ChordMessage, msg.len, 0, info, &len);
  ChordMessage *recvd_chord_msg =
      chord_message__unpack(NULL, msg.len, msg.ChordMessage);

  free(msg.ChordMessage);
  return recvd_chord_msg;
}

ChordMessage construct_msg(ChordMessage__MsgCase cs, int id, int qid,
                           Entry *optional) {
  ChordMessage *ret = 0;

  switch (cs) {

  case CHORD_MESSAGE__MSG_FIND_SUCCESSOR_REQUEST: {
    ChordMessage cloak;
    FindSuccessorRequest rq;
    find_successor_request__init(&rq);
    chord_message__init(&cloak);
    cloak.version = 417;

    cloak.msg_case = CHORD_MESSAGE__MSG_FIND_SUCCESSOR_REQUEST;
    rq.key = id;
    cloak.find_successor_request = &rq;
    cloak.query_id = qid;

    ret = &cloak;

  } break;
  case CHORD_MESSAGE__MSG_NOTIFY_REQUEST: {
    ChordMessage cloak;
    NotifyRequest rq;
    Node nd;
    node__init(&nd);
    notify_request__init(&rq);
    chord_message__init(&cloak);
    cloak.version = 417;
    cloak.msg_case = CHORD_MESSAGE__MSG_NOTIFY_REQUEST;
    cloak.query_id = qid;
    nd.key = optional->key;
    nd.port = optional->port;
    nd.address = optional->address;
    cloak.notify_request = &rq;
    rq.node = &nd;
    ret = &cloak;
  } break;
  case CHORD_MESSAGE__MSG_NOTIFY_RESPONSE: {
    ChordMessage cloak;
    NotifyResponse rq;

    notify_response__init(&rq);
    chord_message__init(&cloak);
    cloak.version = 417;
    cloak.msg_case = CHORD_MESSAGE__MSG_NOTIFY_RESPONSE;
    cloak.query_id = qid;

    cloak.notify_response = &rq;
  } break;
  case CHORD_MESSAGE__MSG_FIND_SUCCESSOR_RESPONSE: {
    ChordMessage cloak;
    FindSuccessorResponse rq;
    Node nd;
    node__init(&nd);
    find_successor_response__init(&rq);
    chord_message__init(&cloak);
    cloak.version = 417;

    cloak.msg_case = CHORD_MESSAGE__MSG_FIND_SUCCESSOR_RESPONSE;
    rq.key = id;
    nd.key = optional->key;
    nd.port = optional->port;
    nd.address = optional->address;
    rq.node = &nd;
    cloak.find_successor_response = &rq;
    cloak.query_id = qid;
    ret = &cloak;
  } break;
  case CHORD_MESSAGE__MSG_GET_PREDECESSOR_REQUEST: {
    ChordMessage cloak;
    GetPredecessorRequest rq;
    get_predecessor_request__init(&rq);

    chord_message__init(&cloak);
    cloak.version = 417;

    cloak.msg_case = CHORD_MESSAGE__MSG_GET_PREDECESSOR_REQUEST;

    cloak.get_predecessor_request = &rq;
    cloak.query_id = qid;
    ret = &cloak;
  }

  break;
  case CHORD_MESSAGE__MSG_GET_PREDECESSOR_RESPONSE: {
    ChordMessage cloak;
    GetPredecessorResponse rq;
    Node nd;
    get_predecessor_response__init(&rq);
    node__init(&nd);
    chord_message__init(&cloak);
    cloak.version = 417;
    nd.key = optional->key;
    nd.port = optional->port;
    nd.address = optional->address;
    cloak.msg_case = CHORD_MESSAGE__MSG_GET_PREDECESSOR_RESPONSE;
    rq.node = &nd;
    cloak.get_predecessor_response = &rq;
    cloak.query_id = qid;
    ret = &cloak;

  } break;

  case CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_REQUEST: {
    ChordMessage cloak;
    CheckPredecessorRequest rq;
    check_predecessor_request__init(&rq);

    chord_message__init(&cloak);
    cloak.version = 417;

    cloak.msg_case = CHORD_MESSAGE__MSG_GET_PREDECESSOR_REQUEST;

    cloak.check_predecessor_request = &rq;
    cloak.query_id = qid;
    ret = &cloak;

  } break;
  case CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_RESPONSE: {
    ChordMessage cloak;
    CheckPredecessorResponse rq;
    check_predecessor_response__init(&rq);

    chord_message__init(&cloak);
    cloak.version = 417;

    cloak.msg_case = CHORD_MESSAGE__MSG_GET_PREDECESSOR_REQUEST;

    cloak.check_predecessor_response = &rq;
    cloak.query_id = qid;
    ret = &cloak;
  } break;
  case CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_REQUEST: {
    ChordMessage cloak;
    chord_message__init(&cloak);
    GetSuccessorListRequest rq;
    get_successor_list_request__init(&rq);
    cloak.msg_case = CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_REQUEST;
    cloak.query_id = qid;
    cloak.get_successor_list_request = &rq;
    ret = &cloak;

  } break;
  case CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_RESPONSE: {
    // successor_list does not exist need optional argument.
    Entry *successor_list = successor_list;
    ChordMessage cloak;
    GetSuccessorListResponse rsp;
    chord_message__init(&cloak);
    get_successor_list_response__init(&rsp);
    rsp.n_successors = entry_list_size(successor_list);
    rsp.successors = malloc(sizeof(Entry) * rsp.n_successors);
    Entry *curr_successor = successor_list->next;
    for (int i = 0; i < rsp.n_successors; i++) {
      rsp.successors[i]->key = curr_successor->key;
      rsp.successors[i]->address = curr_successor->address;
      rsp.successors[i]->port = curr_successor->port;
    }
    cloak.get_successor_list_response = &rsp;
    cloak.query_id = qid;
    cloak.msg_case = CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_RESPONSE;
    ret = &cloak;
  } break;
  default:
    break;
  }
  return *ret;
}

#endif
