#include "queue.h"
#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>
typedef struct Node Node;
struct Node {

  Node *next;
  Node *prev;
  uint64_t key;
  uint32_t address;
  uint32_t port;
};

Node find_successor(Node client, Node successor, uint64_t id,
                    Node *finger_list);
Node closest_preceding_node(Node client, uint64_t id, Node *finger_list);
