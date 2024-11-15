#include "../include/chord.h"
#include "../include/chord_arg_parser.h"
#include "../include/comms.h"
#include "../include/entry.h"
#include "../include/entry_list.h"
#include "../include/hash.h"
#include "../include/request_queue.h"
#include "../include/time_utils.h"
#include <arpa/inet.h>
#include <assert.h>
#include <endian.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

/**
 * Hanging Questions:
 * - If join fails at an intermediate node (a node other than the "join" node),
 * do we just transmit indefinitely? Or create our own ring?
 * - If we detect a node failure because a request timeouts, what should we do
 * about the other requests that were sent to the failed node
 * - If the same node appears in the finger table and/or successor list  more
 * than once and is the best predecessor of a key, does it count as both the
 * first, second, ..., nth best predecessor or just the first best predecessor?
 * - Should retransmissions be calling the {request}_send functions or just
 * retransmitting the same exact message to the same receiver? The difference is
 * that if we call {request}_send function, the message content/receiver may be
 * different due to updated state since previous transmission
 * - How long should we wait before sending requests to next best predecessor
 * after determining a node has failed
 * - In stabilize, would it be fine if we stored the new successor in old
 * successor list before notify and get_successor_list
 * - Are we hashing correctly?
 * - when do we reset timeout for periodic updates
 * - Consider case when we retransmit all the time because we forget to check if
 * the node we're sending to is ourselves
 */

#define STDIN 0
#define MAX_BYTES 50
#define RETRANSMIT_TIMEOUT_SECS 10
#define TRANSMISSION_ATTEMPTS 2
#define MOD_M 64
void printKey(uint64_t key) { printf("%" PRIu64 "\n", key); }

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

  double time_to_next_action = get_time_diff_now_secs(next_action_timeval);
  if (time_to_next_action < 0) {
    timeout->tv_sec = 0;
    timeout->tv_usec = 0;
  } else {
    secs_to_timeval(time_to_next_action, timeout);
  }

  return action_flag;
}

// Determine if req has passed the given RTO since the start of the request
// And if we should restart it or not based on given number of (re)transmissions
// allowed Returns 0 if not timed out (or local), 1 if timed out and to be
// retransmitted, 2 if timed out and out of attempts. Note: n_transmissions = 1
// means no retransmissions (original transmission counts in n_transmissions)
int request_timed_out(OutstandingRequest *req, double retransmission_timeout,
                      int n_transmissions) {
  // Local request can't timeout
  if (req->query_id == -1) {
    return 0;
  }
  if (get_time_diff_now_secs(&req->start_time) >= retransmission_timeout) {
    if (req->nth_transmission >= n_transmissions) {
      return 2;
    } else {
      return 1;
    }
  }
  return 0;
}

void retransmit_request(OutstandingRequest *req, int sock) {
  send_message(req->sent_message, sock, req->dest_addr);
  get_timeval_now(&req->start_time);
  req->nth_transmission++;
}

// TODO: if this node is in finger table/successor list, we remove it.
// But if we do, what values should we replace it with (push everything down one
// to fix gap for succ list?) What about finger list? let fix_fingers eventually
// fill the gap?
void handle_node_failure(int sock, Entry *client, OutstandingRequest *req,
                         Entry *finger_table, Entry *successor_list,
                         Entry **predecessor, struct chord_arguments *args,
                         OutstandingRequest *request_queue) {
  uint8_t *concatenation;

  concatenate_port_addr((uint8_t *)&args->join_address.sin_port,
                        (uint8_t *)&args->join_address.sin_addr.s_addr,
                        concatenation);
  struct sha1sum_ctx *ctx = sha1sum_create(NULL, 0);
  uint8_t extended_key[32];
  sha1sum_finish(ctx, concatenation,
                 sizeof(args->my_address.sin_addr) +
                     sizeof(args->my_address.sin_port),
                 extended_key);
  uint64_t newk = sha1sum_truncated_head(extended_key);
  sha1sum_reset(ctx);

  // If we delete something, we know that the best predecessors has
  // shifted once. For example, the 2nd best predecessor is now best predecessor
  // if we deleted the best predecessor. If we don't delete anything, we know we
  // have to now find the next best predecessor (e.g instead of best
  // predecessor, we send to next (2nd) best predecessor). e.g. node n sends
  // find_successor_request to predecessor p, where p is in our successor/finger
  // list. p doesn't respond and is determined to have failed after not
  // responding to any (re)transmissions. We remove any instances of it in the
  // lists, and what was once the 2nd best predecessor p_2 is now the best
  // predecessor, so we send to our best predecessor (x = 1). e.g. node n sends
  // find_successor_request to predecessor f, which we received from a response
  // to the previous find_successor_request from best local predecessor p (local
  // = best predecessor in n's lists). f is not in either the successor nor
  // finger list. If we determine f fails after not responding to
  // re(transmissions), we don't want to send another find_successor_request to
  // the best local predecessor p since there is a problem with its node path
  // currently, so we find the 2nd best local predecessor
  if (delete_entries_by_key(finger_table, req->node_id) ||
      delete_entries_by_key(successor_list, req->node_id)) {
    // req->nth_closest_predecessor;
  } else {
    // I'm assuming the join node will never be in the finger table or successor
    // list before the join response is received
    req->nth_closest_predecessor++;
  }
  switch (req->message_type) {

  case join_find_successor:
    // Node associated with join address failed, meaning we revert to creating
    // our own chord ring
    if (req->node_id == newk) {
      break;
      // The failed request was an intermediate find_successor_request, so
      // initial node we sent to isn't the failed node
    } else {
      find_successor(*client, req->input_id, finger_table, successor_list,
                     request_queue, join_find_successor, req->finger_index,
                     sock, req->nth_closest_predecessor);
    }
    delete_request(request_queue, req);
    break;
  case fix_fingers_find_successor:
    find_successor(*client, req->input_id, finger_table, successor_list,
                   request_queue, fix_fingers_find_successor, req->finger_index,
                   sock, req->nth_closest_predecessor);
    delete_request(request_queue, req);
    break;
  case stabilize_get_predecessor: {
    // Nothing to be done.
    // We've already removed it from the successor list and finger table right
    // before this switch statement
    Entry *entry = successor_list->next;
    if (entry == successor_list) {
      // What do we do if we've run out of successors in successor_list
    }
    struct sockaddr_in new_successor_addr;
    new_successor_addr.sin_family = AF_INET;
    new_successor_addr.sin_addr.s_addr = entry->address;
    new_successor_addr.sin_port = entry->port;
    send_message(req->sent_message, sock,
                 (struct sockaddr *)&new_successor_addr);
    OutstandingRequest *new_req = construct_outstanding_request(
        generate_query_id(), get_pred, -1, req->sent_message,
        (struct sockaddr *)&new_successor_addr, 1);
    push_request(request_queue, new_req);
    free(new_req);
    delete_request(request_queue, req);
  } break;
  case user_lookup:
    find_successor(*client, req->input_id, finger_table, successor_list,
                   request_queue, user_lookup, req->finger_index, sock,
                   req->nth_closest_predecessor);
    delete_request(request_queue, req);
    break;
  case check_pred:
    *predecessor = NULL;
    delete_request(request_queue, req);
    break;
  case notify_send: {
    Entry *entry = successor_list->next;
    struct sockaddr_in new_successor_addr;
    new_successor_addr.sin_family = AF_INET;
    new_successor_addr.sin_addr.s_addr = entry->address;
    new_successor_addr.sin_port = entry->port;
    send_notif(client, -1, entry->key, (struct sockaddr *)&new_successor_addr,
               sock, request_queue);
    delete_request(request_queue, req);
  } break;
  case get_pred: {
    Entry *entry = successor_list->next;
    struct sockaddr_in new_successor_addr;
    if (entry == successor_list) {
      // What do we do if we've run out of successors in successor_list
    }
    new_successor_addr.sin_family = AF_INET;
    new_successor_addr.sin_addr.s_addr = entry->address;
    new_successor_addr.sin_port = entry->port;
    send_message(req->sent_message, sock,
                 (struct sockaddr *)&new_successor_addr);
    OutstandingRequest *new_req = construct_outstanding_request(
        generate_query_id(), get_pred, -1, req->sent_message,
        (struct sockaddr *)&new_successor_addr, 1);
    push_request(request_queue, new_req);
    free(new_req);
    delete_request(request_queue, req);
  } break;
  case stablize_get_successor_list: {
    Entry *entry = successor_list->next;
    struct sockaddr_in new_successor_addr;
    if (entry == successor_list) {
      // What do we do if we've run out of successors in successor_list
    }
    new_successor_addr.sin_family = AF_INET;
    new_successor_addr.sin_addr.s_addr = entry->address;
    new_successor_addr.sin_port = entry->port;
    // TODO: DETERMINE WHICH MSG_CASE TO SEND.
    // SENDING NOTIFY IS SAFER THAN SENDING GET_PRED
    // there should be no difference
    int qid = generate_query_id();
    send_notif(client, -1, entry->key, (struct sockaddr *)&new_successor_addr,
               sock, request_queue);
    delete_request(request_queue, req);
  } break;

  default:
    break;
  }
}

// Creates a new chord ring. Initializes finger table and successor list values
// to the client itself
void create_chord_ring(Entry *client, Entry **finger_table,
                       Entry **successor_list) {
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
  FD_ZERO(&incoming);

  FD_SET(ssock, &incoming);
  maxfd = ssock;
  FD_SET(STDIN, &incoming);
  if (STDIN > ssock) {
    maxfd = STDIN;
  }

  struct sockaddr_in localaddr = args.my_address;
  int bval = bind(ssock, (struct sockaddr *)&localaddr, sizeof(localaddr));

  char host_buf[256];
  gethostname(host_buf, sizeof(host_buf));
  struct hostent *ent = gethostbyname(host_buf);
  struct in_addr **host_ls = NULL;
  host_ls = (struct in_addr **)ent->h_addr_list;
  localaddr.sin_addr.s_addr = host_ls[0]->s_addr;
  assert(bval >= 0);

  // TODO: setup the node (create/join chord ring, init ring table and successor
  // list, etc)
  // Get id from hash of sum of concatenation of port and address
  // uint8_t *concatenation;

  // concatenate_port_addr((uint8_t *)&localaddr->join_address.sin_port,
  //                    (uint8_t *)&localaddr->join_address.sin_addr.s_addr,
  //                  concatenation);
  // struct sha1sum_ctx *ctx = sha1sum_create(NULL, 0);
  // uint8_t extended_key[20];
  // sha1sum_finish(ctx, concatenation,
  //            sizeof(localaddr->my_address.sin_addr) +
  //             sizeof(localaddr->my_address.sin_port),
  //      extended_key);

  struct sha1sum_ctx *ctx = sha1sum_create(NULL, 0);
  sha1sum_update(ctx, (void *)&localaddr.sin_port, 6);
  uint8_t extended_key[32];
  sha1sum_finish(ctx, (void *)&localaddr.sin_addr.s_addr,
                 sizeof(localaddr.sin_addr.s_addr), extended_key);
  uint64_t key = sha1sum_truncated_head(extended_key);
  sha1sum_reset(ctx);
  sha1sum_destroy(ctx);
  // No need to explicitly create a new chord ring
  // Since creating a ring requires no additional set up besides initializing
  // these variables
  Entry *client = init_entry(key, (struct sockaddr *)&localaddr);
  Entry *predecessor = NULL;
  Entry *finger_table = init_empty_entry_list(client);
  Entry *successor_list = init_empty_entry_list(client);
  int fix_finger_index = 0;
  OutstandingRequest *request_queue = init_queue();

  // Checks if join address was provided. Not sure if it is actually set to NULL
  // when not provided.
  int adrprov = 0;
  for (int i = 0; i < argc; i++) {

    printf("%s\n", argv[i]);

    if (strcmp(argv[i], "--ja") == 0) {
      printf("study the blade\n");
      adrprov = 1;
    }
  }
  if (argc > 3 && adrprov) {

    // uint8_t *concatenation;

    // concatenate_port_addr((uint8_t *)&args->join_address.sin_port,
    //                    (uint8_t *)&args->join_address.sin_addr.s_addr,
    //                  concatenation);
    // struct sha1sum_ctx *ctx = sha1sum_create(NULL, 0);
    // uint8_t extended_key[20];
    // sha1sum_finish(ctx, concatenation,
    //            sizeof(args->join_address.sin_addr) +
    //             sizeof(args->join_address.sin_port),
    //      extended_key);

    struct sha1sum_ctx *ctx = sha1sum_create(NULL, 0);
    sha1sum_update(ctx, (void *)&args.join_address.sin_port, 6);
    uint8_t extended_key[32];
    sha1sum_finish(ctx, (void *)&args.join_address.sin_addr.s_addr,
                   sizeof(args.join_address.sin_addr.s_addr), extended_key);
    uint64_t newk = sha1sum_truncated_head(extended_key);
    sha1sum_reset(ctx);
    sha1sum_destroy(ctx);
    join_chord_ring_send(client, key, newk,
                         (struct sockaddr *)&args.join_address, ssock,
                         request_queue, 1);

    printf("what we hash the chord owner to\n");
    printKey(newk);
    printf("\n");
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

  printf("> ");
  fflush(stdout);
  while (1) {
    char user_input[MAX_BYTES];
    FD_ZERO(&incoming);
    FD_SET(ssock, &incoming);
    FD_SET(STDIN, &incoming);

    // Iterate through local outstanding requests and handle them
    OutstandingRequest *curr_req = request_queue->next;
    OutstandingRequest *incrm = curr_req;
    while (incrm != request_queue) {
      curr_req = incrm;
      incrm = incrm->next;
      if (curr_req->query_id == -1) {
        switch (curr_req->message_type) {
        // we dont need a case for it being local the -1 already makes it local.
        case fix_fingers_find_successor: {

          Entry *old_finger =
              find_entry_at_index(finger_table, curr_req->finger_index);
          // If we don't find a match, we might be misusing local requests.
          Entry *new_finger =
              find_entry_by_key(successor_list, curr_req->node_id);
          update_entry(old_finger, new_finger->key, new_finger->address,
                       new_finger->port);
          delete_request_by_query_id(request_queue, curr_req->query_id);
        } break;
        case join_find_successor: {
          // this should never happen but is handled
          break;
        }
        case user_lookup: {

          Entry *new_finger =
              find_entry_by_key(successor_list, curr_req->node_id);
          char adr[INET_ADDRSTRLEN];
          inet_ntop(AF_INET, &new_finger->address, adr, sizeof(adr));
          printf("this is local huh\n");
          printf("< ");
          printf("%" PRIu64 " %s %u\n", curr_req->input_id, adr,
                 ntohs(new_finger->port));
          delete_request_by_query_id(request_queue, curr_req->query_id);
          printf("> ");
          fflush(stdout);
        }
        default:
          break;
        }
      }
    }

    // Handle timed out requests
    curr_req = request_queue->next;
    while (curr_req != request_queue) {
      switch (request_timed_out(curr_req, RETRANSMIT_TIMEOUT_SECS,
                                TRANSMISSION_ATTEMPTS)) {
      // Not timed out
      case 0:
        break;
      // Timed out and can still attempt to retransmit
      case 1:
        retransmit_request(curr_req, ssock);
        break;
      // Timed out and out of retransmission attempts
      case 2:
        handle_node_failure(ssock, client, curr_req, finger_table,
                            successor_list, &predecessor, &args, request_queue);
        break;
      }
      curr_req = curr_req->next;
    }

    struct timeval timeout;
    int next_periodic_update_flag =
        next_timeout(&next_check_pred_time, &next_fix_fingers_time,
                     &next_stabilize_time, &timeout);

    FD_ZERO(&incoming);

    FD_SET(ssock, &incoming);
    maxfd = ssock;
    FD_SET(STDIN, &incoming);
    if (STDIN > ssock) {
      maxfd = STDIN;
    }

    int s_turn = select(maxfd + 1, &incoming, NULL, NULL, &timeout);

    if (s_turn == -1) {
      perror("select:");
      exit(1);
    }
    // Periodic Updates
    if (s_turn == 0) {
      switch (next_periodic_update_flag) {
      case CHECK_PREDECESSOR:
        if (predecessor != NULL) {
          struct sockaddr_in jaddr;
          jaddr.sin_family = AF_INET;
          jaddr.sin_addr.s_addr = predecessor->address;
          jaddr.sin_port = predecessor->port;

          check_predecessor_send(client, (struct sockaddr *)&jaddr, ssock,
                                 request_queue, check_pred);
        }
        shift_timeval(&next_check_pred_time, args.check_predecessor_period);
        break;
      case FIX_FINGERS:
        // find_successor will require a finger_index
        fix_finger_index++;
        if (fix_finger_index >= MOD_M) {
          fix_finger_index = 0;
        }
        find_successor(*client,
                       client->key + ((uint64_t)pow(2, (fix_finger_index))),
                       finger_table, successor_list, request_queue,
                       fix_fingers_find_successor, fix_finger_index, ssock, 1);
        shift_timeval(&next_fix_fingers_time, args.fix_fingers_period);
        break;
      case STABILIZE:
        if (successor_list->next->key != client->key) {
          // TODO: write a function that converts address and port into struct
          // sockaddr
          struct sockaddr_in jaddr;
          jaddr.sin_family = AF_INET;
          jaddr.sin_addr.s_addr = successor_list->next->address;
          jaddr.sin_port = successor_list->next->port;

          get_predecessor_send(key, (struct sockaddr *)&jaddr, ssock,
                               request_queue, get_pred, -1);
        }
        shift_timeval(&next_stabilize_time, args.fix_fingers_period);

        break;
      }
    } else {

      if (FD_ISSET(STDIN, &incoming)) {
        printf("USER INPUT GET RIGHT HERE\n");
        // TODO: Determine max length of string. For now, assume 100 is enough
        int lookup_length = 6;
        int printstate_length = 10;
        char buf[101];
        int num_bytes = read(STDIN, buf, 100);
        // Read takes in the \n character at the end of stdin. This replaces it
        // with the null terminator
        buf[num_bytes - 1] = '\x00';
        if (strncmp("Lookup", buf, lookup_length) == 0) {
          char *lookup_string = (char *)malloc(num_bytes - lookup_length + 1);
          //+1 and -1 to account for space between lookup and string
          int lookup_string_length = num_bytes - lookup_length - 1;
          memcpy(lookup_string, buf + lookup_length + 1, lookup_string_length);
          lookup_string[lookup_length] = '\x00';
          // Hash the string to get the key
          struct sha1sum_ctx *ctx = sha1sum_create(NULL, 0);
          uint8_t extended_key[32];
          sha1sum_finish(ctx, (uint8_t *)lookup_string, lookup_string_length,
                         extended_key);
          uint64_t truncated_key = sha1sum_truncated_head(extended_key);
          printf("< %s %" PRIu64 "\n", lookup_string, truncated_key);
          printf("THIS IS  THE HASH\n");
          printf("%lu\n", truncated_key);

          sha1sum_destroy(ctx);
          free(lookup_string);

          find_successor(*client, truncated_key, finger_table, successor_list,
                         request_queue, user_lookup, -1, ssock, 1);
        } else if (strncmp("PrintState", buf, printstate_length) == 0) {
          char adr[INET_ADDRSTRLEN];
          inet_ntop(AF_INET, &localaddr.sin_addr.s_addr, adr, sizeof(adr));
          int por = ntohs(localaddr.sin_port);
          printf("< Self %lu %s %u\n", key, adr, por);
          Entry *k;
          int pos = 1;

          for (k = successor_list->next; k != successor_list; k = k->next) {
            char adr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &k->address, adr, sizeof(adr));
            int por = ntohs(k->port);
            printf("< Successor [%u] %lu %s %u\n", pos, k->key, adr, por);
            pos++;
          }
          pos = 1;

          for (k = finger_table->next; k != finger_table; k = k->next) {
            char adr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &k->address, adr, sizeof(adr));
            int por = ntohs(k->port);
            printf("< Finger [%u] %lu %s %u\n", pos, k->key, adr, por);
            pos++;
          }

        } else {
          // invalid user input
        }
      }
      if (FD_ISSET(ssock, &incoming)) {
        struct sockaddr src_addr;
        printf("i think its recv mssage\n");
        ChordMessage *msg = recv_message(ssock, &src_addr);
        printf("i got a msg\n");
        // cases to handle requests
        switch (msg->msg_case) {
        case CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_REQUEST: {
          ChordMessage *msg_send =
              construct_msg(CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_RESPONSE, -1,
                            msg->query_id, client);
          send_message(msg_send, ssock, &src_addr);
        } break;
        case CHORD_MESSAGE__MSG_FIND_SUCCESSOR_REQUEST:
          printf("A NODE CONTACTED ME!!!!\n");
          // Might need a separate Msg_Type for the case where find_successor
          // is called bcs of a find_successor request
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

            ChordMessage *sending =
                construct_msg(CHORD_MESSAGE__MSG_FIND_SUCCESSOR_RESPONSE,
                              req_key, msg->query_id, will_send);
            send_message(sending, ssock, &src_addr);
          }
          break;
        case CHORD_MESSAGE__MSG_GET_PREDECESSOR_REQUEST: {
          ChordMessage *msg_send =
              construct_msg(CHORD_MESSAGE__MSG_GET_PREDECESSOR_RESPONSE, -1,
                            msg->query_id, predecessor);
          send_message(msg_send, ssock, &src_addr);
        } break;

        case CHORD_MESSAGE__MSG_NOTIFY_REQUEST: {
          printf("inside notif\n");
          struct NotifyRequest *ls = msg->notify_request;
          Node *tmp = ls->node;
          if (predecessor == NULL ||
              is_between_exclusive(predecessor->key, tmp->key, key)) {
            struct sockaddr_in temp;
            temp.sin_port = (tmp->port);
            temp.sin_addr.s_addr = (tmp->address);
            temp.sin_family = AF_INET;
            Entry *newpred = init_entry(tmp->key, (struct sockaddr *)&temp);
            if (predecessor == NULL) {
              free(predecessor);
            }
            predecessor = newpred;
          }
          ChordMessage *sending = construct_msg(
              CHORD_MESSAGE__MSG_NOTIFY_RESPONSE, -1, msg->query_id, NULL);
          send_message(sending, ssock, &src_addr);
        } break;

        case CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_REQUEST: {
          ChordMessage *msg_send =
              construct_msg(CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_RESPONSE, -1,
                            msg->query_id, successor_list);
          send_message(msg_send, ssock, &src_addr);
        } break;
        // message is not a request, must be a response
        default:
          // TODO:Find the outstanding request related to the received
          // response
          switch (msg->msg_case) {
          case CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_RESPONSE:
            delete_request_by_query_id(request_queue, msg->query_id);
            break;
          case CHORD_MESSAGE__MSG_FIND_SUCCESSOR_RESPONSE: {
            OutstandingRequest *k;
            k = find_request_by_query_id(request_queue, msg->query_id);

            struct FindSuccessorResponse *ls = msg->find_successor_response;

            Node *g_pred = ls->node;
            uint64_t lower = key;
            uint64_t higher = g_pred->key;
            uint64_t mid = k->input_id;
            // we have not found the successor and need to send further

            printKey(lower);

            printKey(mid);
            printKey(higher);

            if (!(is_between_inclusive(lower, mid, higher))) {
              int qid = generate_query_id();
              ChordMessage *msg =
                  construct_msg(CHORD_MESSAGE__MSG_FIND_SUCCESSOR_REQUEST,
                                k->input_id, qid, NULL);

              struct sockaddr_in tmp;
              tmp.sin_port = htonl(g_pred->port);
              tmp.sin_addr.s_addr = htonl(g_pred->address);
              tmp.sin_family = AF_INET;
              send_message(msg, ssock, (struct sockaddr *)&tmp);
              OutstandingRequest *new_req =
                  construct_outstanding_request_with_id(
                      qid, k->message_type, k->finger_index, k->input_id,
                      higher, msg, (struct sockaddr *)&tmp, 1);
              push_request(request_queue, new_req);
              delete_request_by_query_id(request_queue, msg->query_id);
              free_outstanding_request(new_req);
              // push_entry(request_queue,new_req);
              // delete_entry_at_index(request_queue,rq_index);
              // free_request(k);

            } else {

              struct sockaddr_in temp;
              temp.sin_port = g_pred->port;
              temp.sin_addr.s_addr = g_pred->address;
              temp.sin_family = AF_INET;
              Entry *pred = init_entry(g_pred->key, (struct sockaddr *)&temp);
              delete_request_by_query_id(request_queue, msg->query_id);

              switch (k->message_type) {

              case (fix_fingers_find_successor): {

                handle_fix_fingers_find_successor_response(
                    finger_table, k->finger_index, pred);
              } break;
              case (join_find_successor): {

                push_entry(finger_table, pred);
                push_entry(successor_list, pred);

                break;
              }
              case (user_lookup): {

                char adr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &g_pred->address, adr, sizeof(adr));
                printf("the magic arts\n");
                printf("< ");
                printf("%" PRIu64 " %s %u\n", pred->key, adr, g_pred->port);
                break;
              }
              default:
                break;
              }
            }

            break;

          } break;
          case CHORD_MESSAGE__MSG_GET_PREDECESSOR_RESPONSE: {

            GetPredecessorResponse *k;
            OutstandingRequest *ctex =
                find_request_by_query_id(request_queue, msg->query_id);
            k = msg->get_predecessor_response;
            uint64_t newprd = k->node->key;
            if (k->node->port != -1) {

              if (is_between_exclusive(key, newprd, ctex->node_id)) {

                update_entry(finger_table->next, k->node->key, k->node->address,
                             k->node->port);
                update_entry(successor_list->next, k->node->key,
                             k->node->address, k->node->port);
              }
            }
            delete_request_by_query_id(request_queue, msg->query_id);
            struct sockaddr_in jaddr;
            jaddr.sin_family = AF_INET;
            jaddr.sin_addr.s_addr = successor_list->next->address;
            jaddr.sin_port = successor_list->next->port;
            send_notif(client, key, successor_list->next->key,
                       (struct sockaddr *)&jaddr, ssock, request_queue);

            break;
          }
          case CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_RESPONSE: {

            // this is for stabilize successor list
            OutstandingRequest *current_request =
                find_request_by_query_id(request_queue, msg->query_id);
            GetSuccessorListResponse *rsp = msg->get_successor_list_response;
            size_t numnode = rsp->n_successors;

            wipe_entry_list(successor_list);
            Entry tmp;

            tmp.key = current_request->node_id;
            struct sockaddr_in *successordat = (struct sockaddr_in *)&src_addr;

            tmp.port = successordat->sin_port;
            tmp.address = successordat->sin_addr.s_addr;
            push_entry(successor_list, &tmp);
            if (numnode >= args.num_successors) {
              numnode = args.num_successors - 1;
            }
            for (int i = 0; (unsigned)i < numnode; i++) {

              Entry tmp;
              tmp.key = rsp->successors[i]->key;
              tmp.port = rsp->successors[i]->address;
              tmp.address = rsp->successors[i]->address;
              push_entry(successor_list, &tmp);
            }
          } break;
          case CHORD_MESSAGE__MSG_NOTIFY_RESPONSE: {
            OutstandingRequest *current_request =
                find_request_by_query_id(request_queue, msg->query_id);
            get_successor_list_send(current_request->dest_addr, ssock,
                                    successor_list->next->key, request_queue,
                                    stablize_get_successor_list);
            delete_request_by_query_id(request_queue, msg->query_id);

            // delete_entry_at_index(Entry *head, int index) pos = 1;
            // free_request(k);
            break;
          }
          default:
            break;
          }
        }
        chord_message__free_unpacked(msg, NULL);
      }
    }
  }

  return 0;
}
