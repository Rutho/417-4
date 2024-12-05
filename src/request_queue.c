#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/outstanding_request.h"
#include "../include/queue.h"
#include "../include/request_queue.h"
OutstandingRequest *init_queue() {
  OutstandingRequest *queue;
  InitDQ(queue, OutstandingRequest);
  return queue;
}

int request_queue_size(OutstandingRequest *queue) {
  OutstandingRequest *curr_req = queue->next;
  int count = 0;
  while (curr_req != queue) {
    count++;
    curr_req = curr_req->next;
  }
  return count;
}

// Pushes shallow copy of request to end of queue (Deep copy may be needed if we
// add pointer fields other than next/prev) Returns pointer to the shallow copy
OutstandingRequest *push_request(OutstandingRequest *queue,
                                 OutstandingRequest *req) {
  OutstandingRequest *last_added = queue->prev;
  OutstandingRequest *req_copy = construct_outstanding_request_with_id(
      req->query_id, req->message_type, req->finger_index, req->input_id,
      req->node_id, req->sent_message, req->dest_addr,
      req->nth_closest_predecessor);
  last_added->next->prev = req_copy;
  req_copy->next = last_added->next;
  last_added->next = req_copy;
  req_copy->prev = last_added;
  return req_copy;
}

// Adds shallow copy of request to given index of queue.
OutstandingRequest *add_request(OutstandingRequest *queue, int index,
                                OutstandingRequest *req) {
  if (index > request_queue_size(queue)) {
    return push_request(queue, req);
  }
  if (index > request_queue_size(queue)) {
    return NULL;
  }
  OutstandingRequest *curr_req = queue->next;
  OutstandingRequest *req_copy = malloc(sizeof(OutstandingRequest));
  memcpy(req_copy, req, sizeof(OutstandingRequest));
  for (int i = 0; i < index; i++) {
    curr_req = curr_req->next;
  }
  req_copy->next = curr_req->next;
  req_copy->prev = curr_req;
  curr_req->next->prev = req_copy;
  curr_req->next = req_copy;
  return req_copy;
}

// Finds and returns the outstanding request with the given query_id
OutstandingRequest *find_request_by_query_id(OutstandingRequest *queue,
                                             int query_id) {
  OutstandingRequest *curr_req = queue->next;
  while (curr_req != queue) {
    if (curr_req->query_id == query_id) {
      return curr_req;
    }
    curr_req = curr_req->next;
  }
  return NULL;
}

// Do NOT use this for local requests (query_id of all local requests are -1)
void delete_request_by_query_id(OutstandingRequest *queue, int query_id) {
  OutstandingRequest *req = find_request_by_query_id(queue, query_id);
  delete_request(queue, req);
}

void delete_request(OutstandingRequest *queue, OutstandingRequest *req) {
  OutstandingRequest *curr_req = queue->next;
  while (curr_req != queue) {
    if (curr_req == req) {
      curr_req->prev->next = curr_req->next;
      curr_req->next->prev = curr_req->prev;
      free_outstanding_request(req);
      return;
    }
    curr_req = curr_req->next;
  }
}

// void delete_request_by_dest_node(OutstandingRequest *queue, int re) {
//   OutstandingRequest *curr_req = queue->next;
//   while (curr_req != queue) {
//     if (curr_req == req) {
//       curr_req->prev->next = curr_req->next;
//       curr_req->next->prev = curr_req->prev;
//       free_outstanding_request(req);
//       return;
//     }
//     curr_req = curr_req->next;
//   }
// }
