#pragma once

#include "thread.h"
#include "process.h"

#define KERNEL_PID ((pid_t)0)


void sched_save(gp_regs_t* context);


// if this function returns, then a fast context restore 
// can happen: no need to unmap/map user space memory
void schedule(void); 


/**
 * return a fresh new pid that 
 * doesn't overlap with an existing process
 * or with a process that just exited
 */
pid_t alloc_pid(void);


void sched_register_process(process_t* process);

