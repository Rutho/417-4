#ifndef _ENTRYH_
#define _ENTRYH_
#include "outstanding_request.h"
#include "queue.h"
#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
typedef struct Entry Entry;

struct Entry {
  Entry *next;
  Entry *prev;
  uint64_t key;
  uint64_t address;
  uint32_t port;
};

void send_notif(Entry *client, int id, int rev_id, struct sockaddr *join_addr,
                int sock, OutstandingRequest *request_queue);
void user_lookup_send(Entry *client, int id, int rev_id,
                      struct sockaddr *join_addr, int sock,
                      OutstandingRequest *request_queue);
void find_successor(Entry client, uint64_t id, Entry *finger_list,
                    Entry *successor_list, OutstandingRequest *request_queue,
                    enum Msg_Type M, int index, int sock);
Entry *closest_preceding_node(Entry client, uint64_t id, Entry *finger_list,
                              Entry *successor_list);
Entry *init_entry(uint64_t key, struct sockaddr *address);
void join_chord_ring_send(Entry *client, int id, int rev_id,
                          struct sockaddr *join_addr, int sock,
                          OutstandingRequest *request_queue);
void fix_finger_find_successor_request_send(Entry *client, int id, int rev_id,
                                            struct sockaddr *join_addr,
                                            int sock,
                                            OutstandingRequest *request_queue,
                                            int index);
void check_predecessor_send(Entry *client, struct sockaddr *dest_addr, int sock,
                            OutstandingRequest *request_queue, Msg_Type M);
void get_predecessor_send(uint64_t id, struct sockaddr *dest_addr, int sock,
                          OutstandingRequest *request_queue, Msg_Type M,
                          int finger_index);
void handle_get_predecessor_request(Entry *predecessor, int sock,
                                    struct sockaddr *dest_addr);
void stabilize_send(uint64_t id, struct sockaddr *dest_addr, int sock,
                    OutstandingRequest *request_queue);

int is_between_inclusive(uint64_t lower, uint64_t id, uint64_t higher);
int is_between_exclusive(uint64_t lower, uint64_t id, uint64_t higher);
void handle_fix_fingers_find_successor_response(Entry *finger_table,
                                                int finger_index,
                                                Entry *resp_node);
void update_entry(Entry *entry, uint64_t key, uint64_t address, uint32_t port);

#endif
