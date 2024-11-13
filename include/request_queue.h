#ifndef REQUEST_QUEUE_H
#define REQUEST_QUEUE_H

#include "outstanding_request.h"

OutstandingRequest *init_queue();
int request_queue_size(OutstandingRequest *queue);
OutstandingRequest *push_request(OutstandingRequest *queue,
                                 OutstandingRequest *req);
OutstandingRequest *add_request(OutstandingRequest *queue, int index,
                                OutstandingRequest *req);
OutstandingRequest *find_request_by_query_id(OutstandingRequest *queue,
                                             int query_id);
void delete_request(OutstandingRequest *queue, OutstandingRequest *req);
void delete_request_by_query_id(OutstandingRequest *queue, int query_id);

#endif
