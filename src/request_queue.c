#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "outstanding_request.h"
#include "queue.h"

OutstandingRequest *init_queue() {
  OutstandingRequest *queue;
  InitDQ(queue, OutstandingRequest);
  return queue;
}

OutstandingRequest *request_queue_size(OutstandingRequest *queue) {
  OutstandingRequest *curr_req = queue->next;
  int count = 0;
  while (curr_req != queue) {
    count++;
    curr_req = curr_req->next;
  }
  return count;
}

//Pushes shallow copy of request to end of queue (Deep copy may be needed if we add pointer fields other than next/prev)
//Returns pointer to the shallow copy 
OutstandingRequest *push_request(OutstandingRequest *queue, OutstandingRequest *req) {
  OutstandingRequest *last_added = queue->prev;
  OutstandingRequest *req_copy = malloc(sizeof(OutstandingRequest));
  memcpy(req_copy, req, sizeof(OutstandingRequest));
  last_added->next->prev = req_copy;
  req_copy->next = last_added->next;
  last_added->next = req_copy;
  req_copy->prev = last_added;
  return req_copy;
}

//Adds shallow copy of request to given index of queue.
OutstandingRequest *add_request(OutstandingRequest *queue, int index, OutstandingRequest *req) {
  if (index > request_queue_size(queue)) {
    return push_request(queue, req);
  }
  if (index > entry_list_size(queue)) {
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

//Finds and returns the outstanding request with the given query_id
OutstandingRequest *find_request_by_query_id(OutstandingRequest *queue, int query_id) {
  OutstandingRequest *curr_req = queue->next;
  while (curr_req != queue) {
    if (curr_req->query_id == query_id) {
      return curr_req;
    }
    curr_req = curr_req->next;
  }
  return NULL;
}

//Do NOT use this for local requests (query_id of all local requests are -1)
void delete_request_by_query_id(OutstandingRequest *queue, int query_id) {
  OutstandingRequest *req = find_request_by_query_id(queue, query_id);
  delete_req(queue, req);
}

void delete_request(OutstandingRequest *queue, OutstandingRequest *req) {
  OutstandingRequest *curr_req = queue->next;
  while (curr_req != queue) {
    if (curr_req == req) {
      curr_req->prev->next = curr_req->next;
      curr_req->next->prev = curr_req->prev;
      return;
    }
    curr_req = curr_req->next;
  }
}
