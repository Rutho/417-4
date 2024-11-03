#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>

void get_timeval_now(struct timeval *time);

double timeval_to_secs(struct timeval *time);

void secs_to_timeval(double time_secs, struct timeval *time);

double get_time_diff_secs(struct timeval *time1, struct timeval *time2);

double get_time_diff_secs_now(struct timeval *time1);