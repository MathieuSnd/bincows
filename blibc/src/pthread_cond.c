#include <pthread.h>
#include <bc_extsc.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>

#define COND_VAL 0xC0DC0D

int pthread_cond_init(pthread_cond_t* __restrict cond,
			      const pthread_condattr_t* __restrict cond_attr
) {
    *cond = COND_VAL;
}

/* Destroy condition variable COND.  */
int pthread_cond_destroy(pthread_cond_t* cond) {
    // nothing to be destroyed
}

/* Wake up one thread waiting for condition variable COND.  */
int pthread_cond_signal(pthread_cond_t* cond) {
    futex_wake(cond, 1);
}

int pthread_cond_broadcast(pthread_cond_t* cond) {
    futex_wake(cond, -1);
}

/* Wait for condition variable COND to be signaled or broadcast.
   MUTEX is assumed to be locked before.

   This function is a cancellation point and therefore not marked with
  .  */
int pthread_cond_wait(pthread_cond_t* __restrict cond,
			      pthread_mutex_t* __restrict mutex
) {
    pthread_mutex_unlock(mutex);

    futex_wait(cond, COND_VAL, -1);

    pthread_mutex_lock(mutex);
}

