#include "../include/outstanding_request.h"

//finger_index only relevant for fix_finger request type. finger_index won't be read when handling any other message types
OutstandingRequest *construct_outstanding_request(int query_id, ChordMessage__MsgCase message_type, int finger_index) {
    OutstandingRequest *out_req = (OutstandingRequest *)malloc(sizeof(OutstandingRequest));
    out_req->query_id = query_id;
    out_req->message_type = message_type;
    out_req->finger_index = finger_index;
    out_req->next = out_req->prev = out_req;
    return out_req;
}