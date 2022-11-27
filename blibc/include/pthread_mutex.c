#include <pthread.h>
#include <bc_extsc.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>



/* Initialize a mutex.  */
int pthread_mutex_init(pthread_mutex_t* mutex,
			       const pthread_mutexattr_t* mutexattr) 
{
    int type = PTHREAD_MUTEX_DEFAULT;

    if(mutexattr) {
        switch(mutexattr->type) {
            // valid values
            case PTHREAD_MUTEX_DEFAULT:
            //case PTHREAD_MUTEX_NORMAL:
            case PTHREAD_MUTEX_RECURSIVE:
                break;
            // invalid values
            default:
                return EINVAL;
        }
        type = mutexattr->type;
    }

    *mutex = (pthread_mutex_t) {
        .owner = 0,
        .rec   = 0,
        .taken = 0,
        .type  = type,
    };

    return 0;
}


/* Destroy a mutex.  */
int pthread_mutex_destroy(pthread_mutex_t* mutex) {
    // nothing to free
}


/* Try locking a mutex.  */
int pthread_mutex_trylock(pthread_mutex_t* mutex) {
    if(mutex->type == PTHREAD_MUTEX_RECURSIVE && mutex->owner == _get_tid()) {
        mutex->rec++;
        return 0;
    }

    volatile int* taken = (volatile int*)&mutex->taken;


    if(!__sync_bool_compare_and_swap(taken, 0, 1)) {
        // taken already
        return EBUSY;
    }


    if(mutex->type == PTHREAD_MUTEX_RECURSIVE) {
        mutex->owner = _get_tid();
        mutex->rec = 1;
    }

    return 0;
}

/* Lock a mutex.  */
int pthread_mutex_lock(pthread_mutex_t* mutex) {

    if(mutex->type == PTHREAD_MUTEX_RECURSIVE && mutex->owner == _get_tid()) {
        mutex->rec++;
        return 0;
    }

    volatile int* taken = (volatile int*)&mutex->taken;

    while(!__sync_bool_compare_and_swap(taken, 0, 1))
        sleep(0);

    if(mutex->type == PTHREAD_MUTEX_RECURSIVE) {
        mutex->owner = _get_tid();
        mutex->rec = 1;
    }

    return 0;
}


/* Unlock a mutex.  */
int pthread_mutex_unlock(pthread_mutex_t* mutex) {

    if(mutex->type == PTHREAD_MUTEX_RECURSIVE) {
        mutex->rec--;
        if(!mutex->rec) {
            mutex->owner = 0;
            mutex->taken = 0;
        }
    }
    else {
        mutex->taken = 0;
    }

    return 0;
}



int pthread_mutexattr_init(pthread_mutexattr_t* attr) {
    if(!attr)
        return EINVAL;

    
    *attr = (pthread_mutexattr_t) {
        .type = PTHREAD_MUTEX_DEFAULT,
    };
    return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t* attr) {
    (void) attr;
}


int pthread_mutexattr_getpshared(const pthread_mutexattr_t* 
					 __restrict attr,
					 int *__restrict pshared
) {
    *pshared = PTHREAD_PROCESS_PRIVATE;
}

int pthread_mutexattr_setpshared(pthread_mutexattr_t* attr,
					 int pshared
) {
    switch(pshared) {
        case PTHREAD_PROCESS_PRIVATE:
            return 0;
        case PTHREAD_PROCESS_SHARED:
            return ENOTSUP;
        default:
            return EINVAL;
    }
}


int pthread_mutexattr_getprotocol(const pthread_mutexattr_t* 
					  __restrict attr,
					  int *__restrict protocol
) {
    *protocol = PTHREAD_PRIO_NONE;
}

int pthread_mutexattr_setprotocol(pthread_mutexattr_t* attr,
					  int protocol
) {
    switch(protocol) {
        case PTHREAD_PRIO_NONE:    return 0;
        case PTHREAD_PRIO_INHERIT: return ENOTSUP;
        case PTHREAD_PRIO_PROTECT: return ENOTSUP;
        default:                   return EINVAL;
    }
}


/* Return in *PRIOCEILING the mutex prioceiling attribute in *ATTR.  */
int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t* 
					     __restrict attr,
					     int *__restrict prioceiling
) {
    *prioceiling = 0;
}

/* Set the mutex prioceiling attribute in *ATTR to PRIOCEILING.  */
int pthread_mutexattr_setprioceiling(pthread_mutexattr_t* attr,
					     int prioceiling
) {
    switch(prioceiling) {
        case 0:  return 0;
        default: return EINVAL;
    }
}


/* Set the robustness flag of the mutex attribute ATTR.  */
int pthread_mutexattr_setrobust(pthread_mutexattr_t* attr,
					int robustness
) {
    switch(robustness) {
        case PTHREAD_MUTEX_STALLED: return 0;
        case PTHREAD_MUTEX_ROBUST:  return ENOTSUP;
        default:                    return EINVAL;

    }
}

int pthread_mutexattr_getrobust(const pthread_mutexattr_t *attr,
					int *robustness
) {
    *robustness = PTHREAD_MUTEX_STALLED;
}