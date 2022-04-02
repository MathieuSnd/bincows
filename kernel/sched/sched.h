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


// create process and return its pid
// or 0 if unsuccessful
pid_t sched_create_process(pid_t ppid, const void* elffile, size_t elffile_sz);


void sched_register_process(process_t* process);


/**
 * @brief return the current process
 * or NULL if the current task executing
 * is the kernel
 */
process_t* sched_current_process(void);



// return NULL if no such pid
// the process belongs to the scheduler,
// the caller cannot free it
process_t* sched_get_process(pid_t pid);

pid_t sched_current_pid(void);
tid_t sched_current_tid(void);


// return the current task's kernel stack
// this function is to call in the syscall
// routine to load the right stack 
void* sched_task_kernel_stack(void);
