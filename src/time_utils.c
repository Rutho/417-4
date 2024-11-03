#ifndef _TIME_UTILS_C_
#define _TIME_UTILS_C_

#include "time_utils.h"

//sets the timeval given to the current time
void get_timeval_now(struct timeval *time) {
	struct timespec time_now_timespec;
    memset(&time_now_timespec, 0, sizeof(struct timespec));
    clock_gettime(CLOCK_REALTIME, &time_now_timespec);
	memset(time, 0, sizeof(struct timeval));
	time->tv_sec = time_now_timespec.tv_sec;
	time->tv_usec = time_now_timespec.tv_nsec * 1E-3;
    return;
}

//Returns the timeval values in seconds
double timeval_to_secs(struct timeval *time) {
    return time->tv_sec + time->tv_usec * 1E-6;
}

//Sets the field values of timeval provided to time_secs
void secs_to_timeval(double time_secs, struct timeval *time) {
    time->tv_sec = (int) time_secs;
    time->tv_usec = (time_secs - ((int) time_secs)) * 1E6;
    return;
}

//Get the difference between time1 and time2. 
//Returns time1 - time2 in secs with floating-point precision of double.
double get_time_diff_secs(struct timeval *time1, struct timeval *time2) {
    double time1_secs = timeval_to_secs(time1);
    double time2_secs = timeval_to_secs(time2);
    return time1_secs - time2_secs;
}

//Returns time that has passed from time1 to the time now (time_now - time1)
double get_time_diff_now_secs(struct timeval *time1) {
    struct timeval now;
    get_timeval_now(&now);
    double now_secs = timeval_to_secs(&now);
    double time1_secs = timeval_to_secs(time1);
    return now_secs - time1_secs;
}

void get_future_timestamp(struct timeval *time, double secs) {
    get_timeval_now(time);
    shift_timeval(time, secs);
}

void shift_timeval(struct timeval *time, double secs) {
    time->tv_sec += (int) secs;
    time->tv_usec += secs - ((int) secs);
}

#endif