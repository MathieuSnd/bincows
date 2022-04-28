#pragma once

#include <stdint.h>
#include "../lib/stacktrace.h"
#include "../lib/logging.h"

typedef uint32_t spinlock_t;


// fast spinlocks use the same functions as the normal spinlocks
typedef union {uint32_t val; uint8_t reserved[128];} __attribute__((aligned(128))) fast_spinlock_t;


static inline void spinlock_init(spinlock_t* lock) {
    *lock = 0;
}


// should take either a spinlock_t* or a fast_spinlock_t*
void _spinlock_acquire(void *lock);
void _spinlock_release(void *lock);


static inline void spinlock_acquire(void* lock) { 
    
    //log_warn("spinlock_acquire(%lx)", (uint64_t)lock);
    //stacktrace_print();
    //log_warn("---------------------------");
    
    _spinlock_acquire(lock);
}

static inline void spinlock_release(void* lock) {

    //log_warn("spinlock_release(%lx)", (uint64_t)lock);
    //stacktrace_print();
    //log_warn("---------------------------");
    

    _spinlock_release(lock);
}




