#pragma once

#include "thread.h"
#include "process.h"

#define KERNEL_PID ((pid_t)0)
#define MAX_PID 0xffff



#define SLEEP_IRQ 46
#define YIELD_IRQ 47



// the caller should disable interrupts
void sched_save(gp_regs_t* context);


// inits the scheduler.
// shed_cleanup should be called afterwards
// the scheduler should then be started
// before doing stuff
void sched_init(void);

// start the scheduler.
// sched_init should have been called beforehand.
// Basically, it marks the scheduler as started so that
// interrupts can trigger the schedule mechanism,
// and performs the very first schedule call.
void sched_start(void);

// this function cleans up the scheduler.
// it is safe to call it even if sched_init or
// sched_start havn't been called already
void sched_cleanup(void);



// every interrupt handler should call
// either sched_irq_ack() or schedule()
void sched_irq_ack(void);


int timer_irq_should_schedule(void);

void __attribute__((noreturn)) schedule(void); 


/**
 * return a fresh new pid that 
 * doesn't overlap with an existing process
 * or with a process that just exited
 */
pid_t alloc_pid(void);


// create process and return its pid
// or -1 if unsuccessful
// in order to create a process, no process should
// initially mapped in memory.
// the created process is mapped and is not unmapped
pid_t sched_create_process(pid_t ppid, const void* elffile, size_t elffile_sz);


// kill and remove the process with pid pid
// and return its exit status
// this function disables interrupts
int sched_kill_process(pid_t pid, int status);


void sched_register_process(process_t* process);


/**
 * @brief return the current process
 * or NULL if the current task executing
 * is the kernel
 * 
 * the returned process is locked. It should
 * be unlocked by the caller, using 
 * spinlock_release(&process->lock)
 */
process_t* sched_current_process(void);



/**
 * @brief lock the process with
 * given pid and return it
 * 
 * this function disables interrupts
 * 
 * return NULL if no such pid
 * the process belongs to the scheduler,
 * the caller cannot free it
 * 
 */
process_t* sched_get_process(pid_t pid);

pid_t sched_current_pid(void);
tid_t sched_current_tid(void);


/**
 * yield the scheduler from the current thread
 * this function invokes an interrupt 
 * 
 */
void sched_yield(void);

// return the current task's kernel stack
// this function is to call in the syscall
// routine to load the right stack 
void* sched_task_kernel_stack(void);
