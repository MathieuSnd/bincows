#include <pthread.h>
#include <bc_extsc.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <stdatomic.h>

#define COND_VAL 0xC0DC0D

int pthread_cond_init(pthread_cond_t* restrict cond,
			      const pthread_condattr_t* restrict cond_attr
) {
    (void) cond_attr;
    *cond = COND_VAL;
    return 0;
}

/* Destroy condition variable COND.  */
int pthread_cond_destroy(pthread_cond_t* cond) {
    (void) cond;
    // nothing to be destroyed
    return 0;
}

/* Wake up one thread waiting for condition variable COND.  */
int pthread_cond_signal(pthread_cond_t* cond) {
    atomic_fetch_add(cond, 1);
    futex_wake(cond, 1);
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t* cond) {
    atomic_fetch_add(cond, 1);
    futex_wake(cond, -1);
    return 0;
}

/* Wait for condition variable COND to be signaled or broadcast.
   MUTEX is assumed to be locked before.

   This function is a cancellation point and therefore not marked with
  .  */

int pthread_cond_wait(pthread_cond_t* restrict cond,
			      pthread_mutex_t* restrict mutex
) {
    volatile uint32_t value = atomic_load(cond);

    pthread_mutex_unlock(mutex);

    int res = futex_wait(cond, /* not altered */ value, -1);

    pthread_mutex_lock(mutex);

    return res;
}
