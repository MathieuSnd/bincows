#pragma once

#include <stdint.h>
#include "../lib/stacktrace.h"

typedef uint32_t spinlock_t;


// fast spinlocks use the same functions as the normal spinlocks
typedef struct {uint32_t val; uint8_t reserved[60];} __attribute__((aligned(64))) fast_spinlock_t;


static inline void spinlock_init(spinlock_t* lock) {
    *lock = 0;
}


// should take either a spinlock_t* or a fast_spinlock_t*
void _spinlock_acquire(void *lock);
void _spinlock_release(void *lock);


static inline void spinlock_acquire(void* lock) { 
    _spinlock_acquire(lock);
}

static inline void spinlock_release(void* lock) {
    _spinlock_release(lock);
}




