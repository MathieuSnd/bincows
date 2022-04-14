#include "thread.h"
#include "../lib/registers.h"
#include "../memory/vmap.h"
#include "../memory/heap.h"

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


    // set the stack frame
    *(uint64_t*)stack_base = 0;
    *((uint64_t*)stack_base + 1) = 0;

    thread->rsp = stack_base + stack_size - sizeof(gp_regs_t) - 16;

    thread->rsp->cs  = USER_CS;
    thread->rsp->ss  = USER_DS;
    thread->rsp->rflags = USER_RF;
    
    thread->rsp->rbp = (uint64_t)(stack_base - 8);

    thread->rsp->rsp = (uint64_t)stack_base + stack_size;

    thread->kernel_stack = (stack_t) {
        .base = malloc(THREAD_KERNEL_STACK_SIZE),
        .size = THREAD_KERNEL_STACK_SIZE,
    };

    thread->type = READY;

    return 0;
}


void free_thread(thread_t* thread) {
    free(thread->kernel_stack.base);
}