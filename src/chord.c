#include "../include/chord.h"
#include "../include/chord_arg_parser.h"
#include "../include/comms.h"
#include "../include/entry.h"
#include "../include/entry_list.h"
#include "../include/hash.h"
#include "time_utils.h"
#include <arpa/inet.h>
#include <assert.h>
#include <endian.h>
#include <math.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#define STDIN 0
#define MAX_BYTES 50
void printKey(uint64_t key) { printf("%" PRIu64, key); }

// Sets next timeout to the closest periodic update to come
// Returns a flag for select to indicate what action (check, fix, stab) to do
// when it times out 0 for check predecessor, 1 for fix fingers, 2 for
// stabilize.
PeriodicUpdateFlag next_timeout(struct timeval *next_check_pred,
                                struct timeval *next_fix_fingers,
                                struct timeval *next_stabilize,
                                struct timeval *timeout) {
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
void create_chord_ring(Entry *client, Entry **finger_table,
                       Entry **successor_list) {
  *finger_table = init_empty_entry_list(client);
  *successor_list = init_empty_entry_list(client);
}

int main(intargc, char *argv[]) {
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
  // Get id from hash of sum of concatenation of port and address
  uint8_t *concatenation;
  concatenate_port_addr((uint8_t *)localaddr.sin_port,
                        (uint8_t *)localaddr.sin_addr.s_addr, &concatenation);
  struct sha1sum_ctx *ctx = sha1sum_create(NULL, 0);
  uint8_t extended_key[20];
  sha1sum_finish(ctx, concatenation,
                 sizeof(localaddr.sin_addr) + sizeof(localaddr.sin_port),
                 extended_key);
  uint64_t key = sha1sum_truncated_head(extended_key);

  // No need to explicitly create a new chord ring
  // Since creating a ring requires no additional set up besides initializing
  // these variables
  Entry *client = init_entry(key, (struct sockaddr *)&localaddr);
  Entry *predecessor = NULL;
  Entry *finger_table = init_empty_entry_list(client);
  Entry *successor_list = init_empty_entry_list(client);
  int fix_finger_index = 0;
  OutstandingRequest request_queue;

  // Checks if join address was provided. Not sure if it is actually set to NULL
  // when not provided.
  if (&args.join_address.sin_addr != NULL) {
    join_chord_ring_send(client, (struct sockaddr *)&args.join_address, ssock,
                         &request_queue);
  }

  // Initialize timevals for periodic updates
  struct timeval time_now;
  get_timeval_now(&time_now);
  struct timeval next_check_pred_time;
  get_future_timestamp(&next_check_pred_time,
                       args.check_predecessor_period * 1E-1);
  struct timeval next_fix_fingers_time;
  get_future_timestamp(&next_fix_fingers_time, args.fix_fingers_period * 1E-1);
  struct timeval next_stabilize_time;
  get_future_timestamp(&next_stabilize_time, args.stablize_period * 1E-1);

  while (1) {
    char user_input[MAX_BYTES];

    // TODO: Iterate through local outstanding requests and handle them

    struct timeval timeout;
    int next_periodic_update_flag =
        next_timeout(&next_check_pred_time, &next_fix_fingers_time,
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
        // find_successor will require a finger_index
        find_successor(*client, client->key + ((int)pow(2, fix_finger_index)),
                       &finger_table, &successor_list, &request_queue,
                       fix_fingers_find_successor, fix_finger_index);
        shift_timeval(&next_fix_fingers_time, args.fix_fingers_period);
        break;
      case STABILIZE:
        if (successor_list->next->key != client->key) {
          // TODO: write a function that converts address and port into struct
          // sockaddr
          get_predecessor_send(client, successor_list->next->address, ssock,
                               &request_queue, stabilize_get_predecessor, 0);
        }
        shift_timeval(&next_stabilize_time, args.fix_fingers_period);
        break;
      }
    } else {
      if (FD_ISSET(STDIN, &incoming)) {
        // TODO: Determine max length of string. For now, assume 100 is enough
        int lookup_length = 6;
        int printstate_length = 10;
        char buf[101];
        int num_bytes = read(STDIN, buf, 100);
        if (strncmp("Lookup", buf, lookup_length)) {
          char *lookup_string = (char *)malloc(num_bytes - lookup_length);
          //+1 and -1 to account for space between lookup and string
          int lookup_string_length = num_bytes - lookup_length - 1;
          memcpy(lookup_string, buf + lookup_length + 1, lookup_string_length);

          // Hash the string to get the key
          struct sha1sum_ctx *ctx = sha1sum_create(NULL, 0);
          uint8_t extended_key[20];
          sha1sum_finish(ctx, lookup_string, lookup_string_length,
                         extended_key);
          uint64_t truncated_key = sha1sum_truncated_head(extended_key);
          find_successor(*client, truncated_key, finger_table, successor_list,
                         &request_queue, user_lookup, -1);
        } else if (strncmp("PrintState", buf, printstate_length)) {
        }
      }
      if (FD_ISSET(ssock, &incoming)) {
        struct sockaddr src_addr;
        struct ChordMessage *msg = recv_message(ssock, &src_addr);

        // cases to handle requests
        switch (msg->msg_case) {
        case CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_REQUEST:
          // Function to handle check_pred request and send check_pred response
          break;
        case CHORD_MESSAGE__MSG_FIND_SUCCESSOR_REQUEST:
          // Might need a separate Msg_Type for the case where find_successor is
          // called bcs of a find_successor request
          /*find_successor()*/
          {
            struct FindSuccessorRequest *ls = msg->find_successor_request;
            uint64_t req_key = ls->key;
            Entry *cessor = find_entry_at_index(finger_table, 0);
            Entry *will_send = NULL;
            if (is_between_inclusive(key, req_key, cessor->key)) {

              will_send = cessor;

            } else {
              will_send = find_closest_predecessor_entry(finger_table, req_key);
            }

            ChordMessage sending =
                construct_msg(CHORD_MESSAGE__MSG_FIND_SUCCESSOR_RESPONSE,
                              req_key, msg->query_id, NULL);
            send_message(&sending, ssock, &src_addr);
          }
          break;
        case CHORD_MESSAGE__MSG_GET_PREDECESSOR_REQUEST:
          /*handle_get_predecessor_request()*/
          break;
        case CHORD_MESSAGE__MSG_NOTIFY_REQUEST: {
          struct NotifyRequest *ls = msg->notify_request;
          Node *tmp = ls->node;
          if (predecessor == NULL ||
              is_between_exclusive(predecessor->key, tmp->key, key)) {
            struct sockaddr_in temp;
            temp.sin_port = htonl(tmp->port);
            temp.sin_addr.s_addr = htonl(tmp->address);
            temp.sin_family = AF_INET;
            Entry *newpred = init_entry(tmp->key, (struct sockaddr *)&temp);
            predecessor = newpred;
          }
          ChordMessage sending = construct_msg(
              CHORD_MESSAGE__MSG_NOTIFY_RESPONSE, -1, msg->query_id, NULL);
          send_message(&sending, ssock, &src_addr);
        } break;
        case CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_REQUEST:
          break;
        // message is not a request, must be a response
        default:
          // TODO:Find the outstanding request related to the received response

          switch (msg->msg_case) {
          case CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_RESPONSE:
            break;
          case CHORD_MESSAGE__MSG_FIND_SUCCESSOR_RESPONSE: {
            OutstandingRequest *k;
            int rq_index = 0;
            for (k = request_queue.next; k != &request_queue; k = k->next) {
              if (k->query_id == msg->query_id) {

                break;
              }
              rq_index++;
            }
            struct FindSuccessorResponse *ls = msg->find_successor_response;

            Node *g_pred = ls->node;
            uint64_t lower = k->node_id;
            uint64_t higher = g_pred->key;
            uint64_t mid = k->input_id;
            // we have not found the predecessor and need to send further
            if (!(is_between_inclusive(lower, mid, higher))) {
              int qid = generate_query_id();
              ChordMessage ms =
                  construct_msg(CHORD_MESSAGE__MSG_FIND_SUCCESSOR_REQUEST,
                                k->input_id, qid, NULL);

              struct sockaddr_in tmp;
              tmp.sin_port = htonl(g_pred->port);
              tmp.sin_addr.s_addr = htonl(g_pred->address);
              tmp.sin_family = AF_INET;
              send_message(&ms, ssock, (struct sockaddr *)&tmp);
              OutstandingRequest *new_req =
                  construct_outstanding_request_with_id(qid, k->message_type,
                                                        k->finger_index,
                                                        k->input_id, higher);
              // push_entry(request_queue,new_req);
              // delete_entry_at_index(request_queue,rq_index);
              // free_request(k);

            } else {

              struct sockaddr_in temp;
              temp.sin_port = htonl(g_pred->port);
              temp.sin_addr.s_addr = htonl(g_pred->address);
              temp.sin_family = AF_INET;
              Entry *pred = init_entry(g_pred->key, (struct sockaddr *)&temp);

              switch (k->message_type) {

              case (fix_fingers_find_successor): {

                insert_entry(finger_table, k->finger_index, pred);

              } break;
              case (join_find_successor): {
                push_entry(finger_table, pred);
              }
              case (user_lookup): {

                char adr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &g_pred->address, adr, sizeof(adr));
                printf("%" PRIu64 " %s\n", pred->key, adr);
                break;
              }
              default:
                break;
              }
            }

            break;

          } break;
          case CHORD_MESSAGE__MSG_GET_PREDECESSOR_RESPONSE:
            break;
          case CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_RESPONSE:

            break;
          case CHORD_MESSAGE__MSG_NOTIFY_RESPONSE: {

            OutstandingRequest *k;
            int rq_index = 0;
            for (k = request_queue.next; k != &request_queue; k = k->next) {
              if (k->query_id == msg->query_id) {
                break;
              }
              rq_index++;
            }

            // delete_entry_at_index(Entry *head, int index)
            // free_request(k);

          } break;
          }
        }
      }
    }
  }

  return 0;
}
