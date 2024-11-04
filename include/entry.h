#ifndef _ENTRYH_
#define _ENTRYH_
#include "queue.h"
#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>
typedef struct Entry Entry;

struct Entry {
  Entry *next;
  Entry *prev;
  uint64_t key;
  uint64_t address;
  uint32_t port;
  int socket;
};

Entry find_successor(Entry client, Entry successor, uint64_t id,
                     Entry *finger_list);
Entry closest_preceding_node(Entry client, uint64_t id, Entry *finger_list);
#endif
