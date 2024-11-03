#ifndef _COMMS_H_
#define _COMMS_H_

typedef enum MessageType {
    NOTIFY_REQUEST= 0,
    NOTIFY_RESPONSE = 1,
    FindSuccessorRequest = 2,
    FindSuccessorResponse = 3,
    GetPredecessorRequest = 4,
    GetPredecessorResponse = 5,
    CheckPredecessorRequest = 6, 
    CheckPredecessorResponse = 7,
    GetSuccessorListRequest = 8,
    GetSuccessorListResponse = 9,
} MessageType;
#endif