#include "thread.h"
#include "sched.h"

#include "../lib/registers.h"
#include "../memory/vmap.h"
#include "../memory/heap.h"

// for freeing a thread stack
#include "../memory/paging.h"
#include "../memory/physical_allocator.h"

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
        .state = BLOCKED,
        .lock = 0,
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



    return 0;
}


void thread_add_exit_hook(thread_t* thread, exit_hook_fun_t hook) {

    thread->exit_hooks = realloc(
        thread->exit_hooks, 
        sizeof(exit_hook_fun_t) * (thread->n_exit_hooks + 1)
    );
    thread->exit_hooks[thread->n_exit_hooks] = hook;
    thread->n_exit_hooks++;
}


void thread_terminate(thread_t* thread, int status) {
    // call exit hooks
    for (size_t i = 0; i < thread->n_exit_hooks; i++)
        thread->exit_hooks[i](thread, status);

    
    free(thread->kernel_stack.base);

    if(thread->exit_hooks)
        free(thread->exit_hooks);


    // @todo free the thread stack

    //unmap_pages(thread->stack.base, thread->stack.size << 12);

}

void thread_pause(void) {
    assert(interrupt_enable());

    process_t* pr = sched_current_process();
    thread_t* thread = sched_get_thread_by_tid(pr, sched_current_tid());
    thread->sig_wait = 1;

    
    do {
        spinlock_release(&pr->lock);
        _sti();

        sched_block();


        pr = sched_current_process();
        thread = sched_get_thread_by_tid(pr, sched_current_tid());
    }
    while((volatile int)thread->sig_wait);


    spinlock_release(&pr->lock);
    _sti();
}
