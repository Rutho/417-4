#include "../include/entry.h"
#include "../include/outstanding_request.h"
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
// newest element in finger_list is first
#include "../protobuf/chord.pb-c.h"

#include "../include//comms.h"
#include "../include/entry_list.h"
#include "../include/request_queue.h"
void find_successor(Entry client, uint64_t id, Entry *finger_list,
                    Entry *successor_list, OutstandingRequest *request_queue,
                    enum Msg_Type M, int index, int sock) {

  Entry *ret = 0;

  uint64_t lower = client.key;

  Entry *k;
  Entry *successor;
  successor = finger_list->next;

  uint64_t higher = successor->key;

  if (is_between_inclusive(lower, id, higher)) {
    ret = successor;
    OutstandingRequest *temp =
        construct_outstanding_request_with_id(-1, M, index, id, client.key);
    push_request(request_queue, temp);

  } else {

    Entry *preced;
    preced = closest_preceding_node(client, id, finger_list, successor_list);
    struct sockaddr_in tmp;
    struct in_addr fa;
    tmp.sin_addr.s_addr = preced->address.sin_addr.s_addr;
    tmp.sin_port = preced->address.sin_port;
    switch (M) {

    case join_find_successor: {
      join_chord_ring_send(&client, id, preced->key, (struct sockaddr *)&tmp,
                           sock, request_queue);

    } break;
    case fix_fingers_find_successor: {
      fix_finger_init_send(&client, id, preced->key, (struct sockaddr *)&tmp,
                           sock, request_queue, index);
    } break;
    case user_lookup: {
      user_lookup_send(&client, id, preced->key, (struct sockaddr *)&tmp, sock,
                       request_queue);
    }
    default:
      break;
    }
  }
}

Entry *closest_preceding_node(Entry client, uint64_t id, Entry *finger_list,
                              Entry *successor_list) {

  Entry *ret = &client;
  Entry *k;

  uint64_t lower = client.key;
  uint64_t finger_id;
  for (k = finger_list->next; k != finger_list; k = k->next) {

    if (is_between_exclusive(lower, k->key, id)) {
      finger_id = k->key;
      ret = k;
      break;
    }
  }
  Entry *j;
  for (j = successor_list->next; j != successor_list; j = j->next) {

    if ((lower < j->key < id) && j->key > id) {

      ret = j;
      break;
    }
  }

  return ret;
}

Entry *init_entry(uint64_t key, struct sockaddr *address) {
  Entry *entry = (Entry *)malloc(sizeof(Entry));
  entry->key = key;
  memcpy(&entry->address, address, sizeof(Entry));
  return entry;
}

void user_lookup_send(Entry *client, int id, int rev_id,
                      struct sockaddr *join_addr, int sock,
                      OutstandingRequest *request_queue) {
  ChordMessage msg = construct_msg(CHORD_MESSAGE__MSG_FIND_SUCCESSOR_REQUEST,
                                   client->key, generate_query_id(), NULL);
  int query_id = send_message(&msg, sock, join_addr);
  OutstandingRequest *new_req = construct_outstanding_request_with_id(
      query_id, user_lookup, -1, id, rev_id);
  push_request(request_queue, new_req);
}
// i am doing iterative so we send out requests for finger lists;;.
void join_chord_ring_send(Entry *client, int id, int rev_id,
                          struct sockaddr *join_addr, int sock,
                          OutstandingRequest *request_queue) {
  ChordMessage msg = construct_msg(CHORD_MESSAGE__MSG_FIND_SUCCESSOR_REQUEST,
                                   client->key, generate_query_id(), NULL);
  int query_id = send_message(&msg, sock, join_addr);
  OutstandingRequest *new_req = construct_outstanding_request_with_id(
      query_id, join_find_successor, -1, id, rev_id);
  push_request(request_queue, new_req);
}

void fix_finger_init_send(Entry *client, int id, int rev_id,
                          struct sockaddr *join_addr, int sock,
                          OutstandingRequest *request_queue, int index) {
  ChordMessage msg =
      construct_msg(CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_REQUEST, id,
                    generate_query_id(), NULL);
  int query_id = send_message(&msg, sock, join_addr);
  OutstandingRequest *new_req = construct_outstanding_request_with_id(
      query_id, fix_fingers_find_successor, index, id, rev_id);
}

void check_predecessor_send(Entry *client, struct sockaddr *dest_addr, int sock,
                            OutstandingRequest *request_queue, Msg_Type M) {
  ChordMessage temp;
  // key not required for message of this type
  temp = construct_msg(CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_REQUEST,
                       client->key, generate_query_id(), NULL);
  int query_id = send_message(&temp, sock, dest_addr);
  OutstandingRequest *new_req = construct_outstanding_request(query_id, M, 0);
  push_request(request_queue, new_req);
}

void get_predecessor_send(uint64_t id, struct sockaddr *dest_addr, int sock,
                          OutstandingRequest *request_queue, Msg_Type M,
                          int finger_index) {
  ChordMessage msg = construct_msg(CHORD_MESSAGE__MSG_GET_PREDECESSOR_REQUEST,
                                   id, generate_query_id(), NULL);
  int query_id = send_message(&msg, sock, dest_addr);
  OutstandingRequest *new_req =
      construct_outstanding_request(query_id, M, finger_index);
  InsertDQ(request_queue, new_req);
}

void handle_get_predecessor_request(Entry *predecessor, int sock,
                                    struct sockaddr *dest_addr) {
  ChordMessage msg = construct_msg(CHORD_MESSAGE__MSG_GET_PREDECESSOR_RESPONSE,
                                   -1, generate_query_id(), predecessor);
  send_message(&msg, sock, dest_addr);
}

void handle_stabilize_predecessor_response(Entry *predecessor) {}

int is_between_inclusive(uint64_t lower, uint64_t id, uint64_t higher) {

  if (lower < higher) {
    if (lower < id && id <= higher) {
      return 1;
    } else {
      return 0;
    }
  } else {
    if (higher < id && id < lower) {
      return 0;
    } else {
      return 1;
    }
  }
}
int is_between_exclusive(uint64_t lower, uint64_t id, uint64_t higher) {
  if (lower < higher) {
    if (lower < id && id < higher) {
      return 1;
    } else {
      return 0;
    }
  } else {
    if (higher < id && id < lower) {
      return 0;
    } else {
      return 1;
    }
  }
}
