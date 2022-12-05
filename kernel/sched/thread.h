#pragma once

#include <stdint.h>
#include <stddef.h>

#include "../lib/assert.h"
#include "../sync/spinlock.h"

#include "process.h"

// this file describes what a thread is


typedef struct stack {
    void* base;
    size_t size;
} stack_t;



typedef struct gp_regs {

// r15 -> r8
    uint64_t ext[8];


    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbx;
    uint64_t rdx;

    
    uint64_t rcx;
    uint64_t rax;
    uint64_t rbp;

    // <uint64_t eror_code>

    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;

} __attribute__((packed)) gp_regs_t;


static_assert_equals(sizeof(gp_regs_t), 160);


struct kernel_task {
    gp_regs_t* regs;
    stack_t stack;
};

typedef 
enum thread_state {
    READY,
    RUNNING,
    BLOCKED
} tstate_t;



typedef struct {
    uint8_t reserved[512];
} __attribute__((aligned(16))) xstate_t;


typedef void (*exit_hook_fun_t)(struct thread* thread, int status);

typedef
struct thread {
    pid_t pid;

    tid_t tid;

    stack_t kernel_stack;

    stack_t stack;
    gp_regs_t* rsp;

    // on x86, this uses to save the extended
    // cotext: x87, SSE, AVX registers and MXCSR
    xstate_t* xcontext;


    tstate_t state;


    // if this field is set, then the thread
    // is to be terminated when it is next 
    // scheduled
    int should_exit;

    // if this field is set, the thread cannot
    // be lazily killed. A signal cannot happen
    // either. It is important to have
    // such a flag to ensure that every system call
    // eventually returns
    int uninterruptible;


    // if this field is set, then the thread
    // should block when it is next 
    // scheduled
    int should_block;


    // if should_exit is set, this field
    // contains the exit status
    int exit_status;

    // non 0 if the thread is currently
    // waiting for a signal to be delivered
    int sig_wait;

    // lapic id of the cpu this 
    // thread is running on
    // this is used to determine
    // which cpu to send an interrupt
    // to when the thread is to terminate
    // this value is only valid when
    // the thread is running
    unsigned running_cpu_id;

    exit_hook_fun_t* exit_hooks;
    size_t n_exit_hooks;


    // this field is set when a futex-waiting thread 
    // is signaled by futex_wake. It is checked and 
    // cleared in futex_wait to check the cause of
    // the sleep().
    int futex_signaled;
    

    // lock for the thread
    spinlock_t lock;
    
} thread_t;

#define KERNEL_THREAD_STACK_SIZE (1024 * 32)


// 0 if the thread cannot be created
int create_thread(thread_t* thread, pid_t pid, void* stack_base, 
        size_t stacs_size, tid_t, uint64_t rip,uint64_t rdi);


// add a hook to be called when the thread exits
void thread_add_exit_hook(thread_t* thread, exit_hook_fun_t hook);

// the right process should be
// locked and mapped before calling
// this function
//
// terminate a thread:
// - call all exit hooks
// - free the thread's data
//     including its stack 
//     and kernel stack
void thread_terminate(thread_t* thread, int status);


// wait for a signal to be delivered
// to the current thread
void thread_pause(void);