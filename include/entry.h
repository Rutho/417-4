#ifndef _ENTRYH_
#define _ENTRYH_
#include "outstanding_request.h"
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

void find_successor(Entry client, uint64_t id, Entry *finger_list,
                    Entry *successor_list, OutstandingRequest *request_queue,
                    enum Msg_Type M);
Entry *closest_preceding_node(Entry client, uint64_t id, Entry *finger_list,
                              Entry *successor_list);
Entry *init_entry(uint64_t key, struct sockaddr *address);
void join_chord_ring(Entry *client, struct sockaddr *join_addr, int sock,
                     OutstandingRequest *request_queue);
void create_chord_ring(Entry *client, Entry *finger_table,
                       Entry *successor_list);
void check_predecessor(Entry *client, struct sockaddr *dest_addr, int sock,
                       OutstandingRequest *request_queue);
void stabilize_send(uint64_t id, struct sockaddr *dest_addr, int sock,
                    OutstandingRequest *request_queue);

#endif
