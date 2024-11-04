#include "../include/chord.h"
#include "../include/chord_arg_parser.h"
#include "../include/hash.h"
#include "time_utils.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#define STDIN 0
void printKey(uint64_t key) { printf("%" PRIu64, key); }

// Sets next timeout to the closest periodic update to come
// Returns a flag for select to indicate what action (check, fix, stab) to do
// when it times out 0 for check predecessor, 1 for fix fingers, 2 for
// stabilize.
int next_timeout(struct timeval *next_check_pred,
                 struct timeval *next_fix_fingers,
                 struct timeval *next_stabilize, struct timeval *timeout) {
  struct timeval *next_action_timeval;
  int action_flag = -1;

  if (get_time_diff_secs(next_check_pred, next_fix_fingers) <= 0) {
    next_action_timeval = next_check_pred;
    action_flag = 0;
  } else {
    next_action_timeval = next_fix_fingers;
    action_flag = 1;
  }

  if (get_time_diff_secs(next_stabilize, next_action_timeval) <= 0) {
    next_action_timeval = next_stabilize;
    action_flag = 2;
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

  // TODO: setup the node (create/join chord ring, init ring table and successor
  // list, etc)

  // Initialize timevals for periodic updates
  struct timeval time_now;
  get_timeval_now(&time_now);
  struct timeval next_check_pred_time;
  get_future_timestamp(&next_check_pred_time, args.check_predecessor_period);
  struct timeval next_fix_fingers_time;
  get_future_timestamp(&next_fix_fingers_time, args.fix_fingers_period);
  struct timeval next_stabilize_time;
  get_future_timestamp(&next_stabilize_time, args.stablize_period);
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
  while (1) {

    struct timeval timeout;
    int next_flag = next_timeout(&next_check_pred_time, &next_fix_fingers_time,
                                 &next_stabilize_time, &timeout);

    int s_turn = select(maxfd + 1, &incoming, NULL, NULL, NULL);
    // switch on this
  }

  return 0;
}
