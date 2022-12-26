#ifndef _SCHED_H
#define _SCHED_H

/*
    bincows scheduler only implements RR scheduling

*/


#ifdef __cplusplus
extern "C" {
#endif

#define SCHED_FIFO  0
#define SCHED_RR    1
#define SCHED_OTHER 2
#define SCHED_BATCH 3
#define SCHED_IDLE  4

int sched_get_priority_max(int policy);
int sched_get_priority_min(int policy);

#include "sys/types.h"

struct sched_param {
    int sched_priority;
};

struct timespec {
    //
};


int sched_getparam(pid_t, struct sched_param *);
int sched_getscheduler(pid_t);
int sched_rr_get_interval(pid_t, struct timespec *);
int sched_setparam(pid_t, const struct sched_param *);
int sched_setscheduler(pid_t, int, const struct sched_param *);
int sched_yield(void);


#ifdef __cplusplus
}
#endif

#endif//_SCHED_H