#include "sched.h"
#include "process.h"
#include "../memory/vmap.h"

/**
 * schedule algorithm:
 *  N ready queues of threads
 * 
 * ready_queue1: most priority queue
 *  ...
 * ready_queueN: least priority queue
 * 
 * 
 * choose_next_task:
 *   for i in[1 ... N]:
 *      t = ready_queue[i].pop()
 *      if t:
 *          up_tasks()
 *          return t
 *     return null
 * 
 * up_tasks
 *   for i in[2 ... N]:
 *      t = ready_queues[i].get()
 *      if t:
 *          ready_queues[i-1].put(t)
 * 
 */


// pid 

static pid_t current_pid = KERNEL_PID;
static tid_t current_tid = 0;


static size_t n_processes = 0;
static size_t n_threads = 0;

static process_t* processes;


#define N_QUEUES 4

// alloc pids in a circular way
static pid_t last_pid = KERNEL_PID;

gp_regs_t saved;

// defined in restore_context.s
void __attribute__((noreturn)) _restore_context(struct gp_regs*);


static 
thread_t* get_thread_by_tid(process_t* process, tid_t tid) {

    // sequencial research 
    for(unsigned i = 0; i < process->n_threads; i++)
        if(process->threads[i].tid == tid)
            return &process->threads[i];

    return NULL;
}




void  sched_save(gp_regs_t* context) {
    saved = *context;
    if(is_user(saved.rip)) {
        //panic("SAVED USER CONTEXT");
    }
}


void schedule(void) {
    _restore_context(&saved);
}



pid_t alloc_pid(void) {
    return ++last_pid;
}
