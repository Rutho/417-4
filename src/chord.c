#include "../include/chord.h"
#include "../include/comms.h"
#include "../include/chord_arg_parser.h"
#include "../include/hash.h"
#include "../include/entry.h"
#include "../include/entry_list.h"
#include "time_utils.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <math.h>
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

// Creates a new chord ring. Initializes finger table and successor list values
// to the client itself
void create_chord_ring(Entry *client, Entry **finger_table, Entry **successor_list) {
  *finger_table = init_empty_entry_list(client);
  *successor_list = init_empty_entry_list(client);
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

  //No need to explicitly create a new chord ring
  //Since creating a ring requires no additional set up besides initializing these variables 
  Entry *client = init_entry(key, (struct sockaddr *)&localaddr);
  Entry *predecessor = NULL;
  Entry *finger_table = init_empty_entry_list(client);
  Entry *successor_list = init_empty_entry_list(client);
  int fix_finger_index = 0;
  OutstandingRequest request_queue;

  //Checks if join address was provided. Not sure if it is actually set to NULL when not provided.
  if (&args.join_address.sin_addr != NULL) {
    join_chord_ring_send(client, (struct sockaddr *)&args.join_address, ssock, &request_queue);
  }

  // Initialize timevals for periodic updates
  struct timeval time_now;
  get_timeval_now(&time_now);
  struct timeval next_check_pred_time;
  get_future_timestamp(&next_check_pred_time, args.check_predecessor_period * 1E-1);
  struct timeval next_fix_fingers_time;
  get_future_timestamp(&next_fix_fingers_time, args.fix_fingers_period * 1E-1);
  struct timeval next_stabilize_time;
  get_future_timestamp(&next_stabilize_time, args.stablize_period * 1E-1);

  while (1) {
    char user_input[MAX_BYTES];

    //TODO: Iterate through local outstanding requests and handle them

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
          if (predecessor) {
            check_predecessor_send(client, predecessor, ssock, &request_queue);
          }
          shift_timeval(&next_check_pred_time, args.check_predecessor_period);
          break;
        case FIX_FINGERS:
          //find_successor will require a finger_index
          find_successor(*client, client->key + ((int) pow(2, fix_finger_index)), 
                        &finger_table, &successor_list, &request_queue, fix_fingers_find_successor);
          shift_timeval(&next_fix_fingers_time, args.fix_fingers_period);
          break;
        case STABILIZE:
          if (successor_list->next->key != client->key) {
            //TODO: write a function that converts address and port into struct sockaddr
            get_predecessor_send(client, successor_list->next->address, 
                                ssock, &request_queue, stabilize_get_predecessor, 0);
          }
          shift_timeval(&next_stabilize_time, args.fix_fingers_period);
          break;
      }
    } else {
      if (FD_ISSET(STDIN, &incoming)) {
        //TODO: Determine max length of string. For now, assume 100 is enough
        int lookup_length = 6;
        int printstate_length = 10;
        char buf[101];
        int num_bytes = read(STDIN, buf, 100);
        if (strncmp("Lookup", buf, lookup_length)) {
          char *lookup_string = (char *)malloc(num_bytes - lookup_length);
          //+1 and -1 to account for space between lookup and string
          int lookup_string_length = num_bytes - lookup_length - 1;
          memcpy(lookup_string, buf + lookup_length + 1, lookup_string_length);

          //Hash the string to get the key
          struct sha1sum_ctx *ctx = sha1sum_create(NULL, 0);
          uint8_t extended_key[20];
          sha1sum_finish(ctx, lookup_string, lookup_string_length, extended_key);
          uint64_t truncated_key = sha1sum_truncated_head(extended_key);
          find_successor(*client, truncated_key, finger_table, successor_list, &request_queue, );
        } else if (strncmp("PrintState", buf, printstate_length)) {

        }
      }
      if (FD_ISSET(ssock, &incoming)) {
        struct sockaddr src_addr;
        struct ChordMessage *msg = recv_message(ssock, &src_addr);
        //cases to handle requests
        switch (msg->msg_case) {
          case CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_REQUEST:
            //Function to handle check_pred request and send check_pred response
            break;
          case CHORD_MESSAGE__MSG_FIND_SUCCESSOR_REQUEST:
            //Might need a separate Msg_Type for the case where find_successor is called bcs of a find_successor request
            /*find_successor()*/
            break;
          case CHORD_MESSAGE__MSG_GET_PREDECESSOR_REQUEST:
            /*handle_get_predecessor_request()*/
            break;
          case CHORD_MESSAGE__MSG_NOTIFY_REQUEST:
            break;
          case CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_REQUEST:
            break;
          //message is not a request, must be a response
          default:
            //TODO:Find the outstanding request related to the received response

            switch (msg->msg_case) { 
              case CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_RESPONSE:
                break;
              case CHORD_MESSAGE__MSG_FIND_SUCCESSOR_RESPONSE:
                break;
              case CHORD_MESSAGE__MSG_GET_PREDECESSOR_RESPONSE:
                break;
              case CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_RESPONSE:
                break;
              case CHORD_MESSAGE__MSG_NOTIFY_RESPONSE:
                break;
            }
          
            
        }
      }
    }
  }

  return 0;
}
