#pragma once

#include <stdint.h>
#include "../lib/stacktrace.h"
#include "../lib/logging.h"
#include "../lib/assert.h"
#include "../lib/registers.h"

typedef volatile uint32_t spinlock_t;


// fast spinlocks use the same functions as the normal spinlocks
typedef volatile union {
    uint32_t val; 
    uint8_t reserved[128];
} __attribute__((aligned(128))) fast_spinlock_t;


static inline void spinlock_init(volatile void* lock) {

    spinlock_t* l = lock;
    *l = 0;
}


// should take either a spinlock_t* or a fast_spinlock_t*
void _spinlock_acquire(volatile void *lock);
void _spinlock_release(volatile void *lock);


static inline void spinlock_acquire(volatile void* lock) { 
    assert(!interrupt_enable());
    //stacktrace_print();
    //log_warn("---------------------------");
    
    _spinlock_acquire(lock);
}

static inline void spinlock_release(volatile void* lock) {
    assert(!interrupt_enable());

    //log_warn("spinlock_release(%lx)", (uint64_t)lock);
    //stacktrace_print();
    //log_warn("---------------------------");
    

    _spinlock_release(lock);
}




