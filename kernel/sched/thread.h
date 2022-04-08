#pragma once

#include <stdint.h>
#include <stddef.h>

#include "../lib/assert.h"

#include "process.h"

// this file describes what a thread is


typedef struct stack {
    void* base;
    size_t size;
} stack_t;


typedef int32_t tid_t;

typedef struct gp_regs {

// r15 -> r8
    uint64_t ext[8];


    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbx;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbp;
    uint64_t rax;
    

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


typedef 
struct thread {
    pid_t pid;

    tid_t tid;

    stack_t kernel_stack;

    stack_t stack;
    gp_regs_t* rsp;

    tstate_t type;

} thread_t;

#define THREAD_KERNEL_STACK_SIZE (1024 * 16)


// 0 if the thread cannot be created
int create_thread(thread_t* thread, pid_t pid, void* stack_base, size_t stacs_size, tid_t);


void free_thread(thread_t* thread);

