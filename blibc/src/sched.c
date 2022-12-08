#include "sched.h"


int sched_get_priority_max(int policy) {
    (void) policy;
    return 0;
}


int sched_get_priority_min(int policy) {
    (void) policy;
    return 0;
}



int sched_getparam(pid_t pid, struct sched_param *param) {
    param->sched_priority = 0;
    return 0;
}


int sched_getscheduler(pid_t pid) {
    (void) pid;
    
    return SCHED_RR;
    
}


int sched_rr_get_interval(pid_t pid, struct timespec *ts) {
    (void) pid;
    (void) ts;
    return 10;
}


int sched_setparam(pid_t pid, const struct sched_param *param) {
    (void) pid;
    if(param->sched_priority == 0)
        return 0;
    return -1;
}


int sched_separamcheduler(pid_t pid, int type, const struct sched_param *param) {
    (void) pid;
    (void) param;
    (void) type;
    return -1;
}


int sched_yield(void) {
    asm volatile("int $47");
    return 0;
}
