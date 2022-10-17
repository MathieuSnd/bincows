#pragma once

#include "../sched/process.h"

/**
 * @brief return the time after which the 
 * next thread should be waken up
 * 
 * @param now 
 * @return uint64_t 
 */
uint64_t next_wakeup(uint64_t now);

/**
 * wake up the threads that are sleeping
 * and should be waken up
 * 
 * This function is non blocking
 * and does not reschedule
 * 
 */
void wakeup_threads(void);

// sleep for around ms miliseconds 
void sleep(unsigned ms);

// try to cancel the sleep of a given thread
// return 0 if the cancellation succeed
// non 0 instead
int sleep_cancel(pid_t pid, tid_t tid);


void cleanup_sleeping_threads(void);