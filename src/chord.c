#include "../include/chord.h"
#include "../include/chord_arg_parser.h"
#include "../include/hash.h"
#include "../include/entry.h"
#include "time_utils.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#define STDIN 0
#define MAX_BYTES 50
void printKey(uint64_t key) { printf("%" PRIu64, key); }

// Sets next timeout to the closest periodic update to come
// Returns a flag for select to indicate what action (check, fix, stab) to do
// when it times out 0 for check predecessor, 1 for fix fingers, 2 for
// stabilize.
PeriodicUpdateFlag next_timeout(struct timeval *next_check_pred,
                 struct timeval *next_fix_fingers,
                 struct timeval *next_stabilize, struct timeval *timeout) {
  struct timeval *next_action_timeval;
  PeriodicUpdateFlag action_flag;

  if (get_time_diff_secs(next_check_pred, next_fix_fingers) <= 0) {
    next_action_timeval = next_check_pred;
    action_flag = CHECK_PREDECESSOR;
  } else {
    next_action_timeval = next_fix_fingers;
    action_flag = FIX_FINGERS;
  }

  if (get_time_diff_secs(next_stabilize, next_action_timeval) <= 0) {
    next_action_timeval = next_stabilize;
    action_flag = STABILIZE;
  }

  double time_to_next_action = get_time_diff_secs_now(next_action_timeval);
  if (time_to_next_action < 0) {
    timeout->tv_sec = 0;
    timeout->tv_usec = 0;
  } else {
    secs_to_timeval(time_to_next_action, timeout);
  }

  return action_flag;
}

int main(int argc, char *argv[]) {
  struct chord_arguments args = chord_parseopt(argc, argv);

  // setting up fd set
  fd_set incoming;
  int maxfd;
  int ssock;
  ssock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  assert(ssock >= 0);
  FD_SET(ssock, &incoming);
  maxfd = ssock;
  struct sockaddr_in localaddr = args.my_address;
  int bval = bind(ssock, (struct sockaddr *)&localaddr, sizeof(localaddr));
  assert(bval >= 0);
  FD_ZERO(&incoming);
  FD_SET(STDIN, &incoming);
  if (STDIN > ssock) {
    maxfd = STDIN;
  }

  // TODO: setup the node (create/join chord ring, init ring table and successor
  // list, etc)
  //Get id from hash of sum of concatenation of port and address
  uint8_t *concatenation;
  concatenate_port_addr((uint8_t *)localaddr.sin_port, (uint8_t *)localaddr.sin_addr.s_addr, &concatenation);
  struct sha1sum_ctx *ctx = sha1sum_create(NULL, 0);
  uint8_t extended_key[20];
  sha1sum_finish(ctx, concatenation, sizeof(localaddr.sin_addr) + sizeof(localaddr.sin_port), extended_key);
  uint64_t key = sha1sum_truncated_head(extended_key);


  Entry *client = init_entry(key, (struct sockaddr *)&localaddr);
  Entry finger_table;
  Entry successor_list;
  OutstandingRequest request_queue;

  //Checks if join address was provided. Not sure if it is actually set to NULL when not provided.
  if (&args.join_address.sin_addr == NULL) {
    create_chord_ring(client, &finger_table, &successor_list);
  } else {
    join_chord_ring(client, (struct sockaddr *)&args.join_address, &request_queue);
  }

  // Initialize timevals for periodic updates
  struct timeval time_now;
  get_timeval_now(&time_now);
  struct timeval next_check_pred_time;
  get_future_timestamp(&next_check_pred_time, args.check_predecessor_period);
  struct timeval next_fix_fingers_time;
  get_future_timestamp(&next_fix_fingers_time, args.fix_fingers_period);
  struct timeval next_stabilize_time;
  get_future_timestamp(&next_stabilize_time, args.stablize_period);

  while (1) {
    char user_input[MAX_BYTES];
    struct timeval timeout;
    int next_periodic_update_flag = next_timeout(&next_check_pred_time, &next_fix_fingers_time,
                                 &next_stabilize_time, &timeout);
    
    int s_turn = select(maxfd + 1, &incoming, NULL, NULL, &timeout);
    if (select == -1) {
      perror("select:");
      exit(1);
    }
    if (select == 0) {
      switch (next_periodic_update_flag) {
        case CHECK_PREDECESSOR:
          check_predecessor(client, successor_list.address, &request_queue);
          break;
        case FIX_FINGERS:
          break;
        case STABILIZE:
          break;
      }
    } else {
      if (FD_ISSET(STDIN, &incoming)) {
        //Handle user input
      }
      if (FD_ISSET(ssock, &incoming)) {
        //receive and handle responses
      }
    }
  }

  return 0;
}
