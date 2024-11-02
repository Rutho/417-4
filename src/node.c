#include "../include/node.h"
#include <stdint.h>
// newest element in finger_list is first
Node find_successor(Node client, Node successor, uint64_t id,
                    Node *finger_list) {

  Node *ret = 0;

  uint64_t lower = client.key;

  uint64_t higher = successor.key;

  if (lower < id <= higher) {
    ret = &successor;
  } else {
    Node sendTo = closest_preceding_node(client, id, finger_list);
    // send successor request over tcp
  }

  return *ret;
}

Node closest_preceding_node(Node client, uint64_t id, Node *finger_list) {

  Node *ret = &client;
  Node *k;
  uint64_t lower = client.key;
  for (k = finger_list->next; k != finger_list; k = k->next) {

    if (lower < k->key < id) {

      ret = k;
      break;
    }
  }

  return *ret;
}
