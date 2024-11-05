#include "../include/entry.h"
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
// newest element in finger_list is first
#include "../protobuf/chord.pb-c.h"

#include "../include//comms.h"
#include "../include/outstanding_request.h"

void find_successor(Entry client, uint64_t id, Entry *finger_list,
                    Entry *successor_list, OutstandingRequest *request_queue,
                    enum Msg_Type M) {

  Entry *ret = 0;

  uint64_t lower = client.key;

  Entry *k;
  Entry *successor;
  successor = finger_list->prev;

  uint64_t higher = successor->key;

  if (lower < id <= higher) {
    ret = successor;
    OutstandingRequest *temp = construct_outstanding_request(-1, M, -1);
    InsertDQ(request_queue, temp);

  } else {
  }
}

Entry *closest_preceding_node(Entry client, uint64_t id, Entry *finger_list,
                              Entry *successor_list) {

  Entry *ret = &client;
  Entry *k;
  uint64_t lower = client.key;
  uint64_t finger_id;
  for (k = finger_list->next; k != finger_list; k = k->next) {

    if (lower < k->key < id) {
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

void join_chord_ring_send(Entry *client, struct sockaddr *join_addr, int sock,
                     OutstandingRequest *request_queue) {
  ChordMessage msg = construct_msg(CHORD_MESSAGE__MSG_FIND_SUCCESSOR_REQUEST,
                                    client->key, generate_query_id(), NULL);
  int query_id = send_message(&msg, sock, join_addr);
  OutstandingRequest *new_req = construct_outstanding_request(
      query_id, join_find_successor, 0);
  InsertDQ(request_queue, new_req);
}

void check_predecessor_send(Entry *client, struct sockaddr *dest_addr, int sock,
                       OutstandingRequest *request_queue, Msg_Type M) {
  ChordMessage temp;
  // key not required for message of this type
  temp = construct_msg(CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_REQUEST,
                       client->key, generate_query_id(), NULL);
  int query_id = send_message(&temp, sock, dest_addr);
  OutstandingRequest *new_req = construct_outstanding_request(
      query_id, M, 0);
  InsertDQ(request_queue, new_req);
}

void get_predecessor_send(uint64_t id, struct sockaddr *dest_addr, int sock, 
                     OutstandingRequest *request_queue, Msg_Type M, int finger_index) {
  ChordMessage msg =
      construct_msg(CHORD_MESSAGE__MSG_GET_PREDECESSOR_REQUEST, id, generate_query_id(), NULL);
  int query_id = send_message(&msg, sock, dest_addr);
  OutstandingRequest *new_req = construct_outstanding_request(
      query_id, M, finger_index);
  InsertDQ(request_queue, new_req);
}

void handle_get_predecessor_request(Entry *predecessor, int sock, struct sockaddr *dest_addr) {
  ChordMessage msg = construct_msg(CHORD_MESSAGE__MSG_GET_PREDECESSOR_RESPONSE, -1, generate_query_id(), predecessor);
  send_message(&msg, sock, dest_addr);
}

void handle_stabilize_predecessor_response(Entry *predecessor) {

}
