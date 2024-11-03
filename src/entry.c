#include "../include/entry.h"
#include <stdint.h>
#include <stdlib.h>
// newest element in finger_list is first
#include "../protobuf/chord.pb-c.h"

#include "../src/comms.h"
Entry find_successor(Entry client, Entry successor, uint64_t id,
                     Entry *finger_list) {

  Entry *ret = 0;

  uint64_t lower = client.key;

  uint64_t higher = successor.key;

  if (lower < id <= higher) {
    ret = &successor;
  } else {
    Entry sendTo = closest_preceding_node(client, id, finger_list);
    // send successor request over tcp
    ChordMessage *temp;
    temp = construct_msg(CHORD_MESSAGE__MSG_FIND_SUCCESSOR_REQUEST, id, NULL);
    send_message(temp, sendTo.socket);
  }

  return *ret;
}

Entry closest_preceding_node(Entry client, uint64_t id, Entry *finger_list) {

  Entry *ret = &client;
  Entry *k;
  uint64_t lower = client.key;
  for (k = finger_list->next; k != finger_list; k = k->next) {

    if (lower < k->key < id) {

      ret = k;
      break;
    }
  }

  return *ret;
}
