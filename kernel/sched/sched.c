#include "sched.h"
#include "process.h"

// pid 

static pid_t current_pid = KERNEL_PID;


// alloc pids in a circular way
static pid_t last_pid = KERNEL_PID;

gp_regs_t saved;

// defined in restore_context.s
void __attribute__((noreturn)) _restore_context(struct gp_regs*);


void  sched_save(gp_regs_t* context) {
    saved = *context;
}


void schedule(void) {
    //_restore_context(&saved);
}



pid_t alloc_pid(void) {
    return ++last_pid;
}
