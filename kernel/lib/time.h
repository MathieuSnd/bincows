#pragma once

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


void cleanup_sleeping_threads(void);