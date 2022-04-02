#include "thread.h"
#include "../lib/registers.h"
#include "../memory/vmap.h"

int create_thread(
            thread_t* thread, 
            pid_t  pid, 
            void*  stack_base, 
            size_t stack_size,
            tid_t  tid
) {
    *thread = (thread_t) {
        .pid   = pid,
        .stack = {
            .base = stack_base,
            .size = stack_size,
        },
        .tid   = tid,
    };

    thread->rsp = stack_base + stack_size - sizeof(gp_regs_t);

    thread->rsp->cs  = USER_CS;
    thread->rsp->ss  = USER_DS;

    thread->rsp->rflags = get_rflags();
    thread->rsp->rbp = 0;

    thread->rsp->rsp = (uint64_t)stack_base + stack_size;

    thread->type = READY;

    return 0;
}