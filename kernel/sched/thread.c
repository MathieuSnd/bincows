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

    thread->regs.cs  = USER_CS;
    thread->regs.ss  = USER_DS;

    thread->regs.rflags = get_rflags();
    thread->regs.rbp = 0;

    thread[0].regs.rsp = stack_base + stack_size;

    printf("issou %lx", thread[0].regs.rsp);

    return 0;
}