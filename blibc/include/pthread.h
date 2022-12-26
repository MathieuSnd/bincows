#ifndef _PTHREAD_H
#define _PTHREAD_H


#ifdef __cplusplus
#define __restrict __restrict__
#else
#define __restrict restrict
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <stddef.h>


typedef int tid_t;


typedef tid_t pthread_t;


typedef struct {
    int joinable;
} pthread_attr_t;


#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1
#define PTHREAD_INHERIT_SCHED 0
#define PTHREAD_EXPLICIT_SCHED 1
#define PTHREAD_SCOPE_SYSTEM 0
#define PTHREAD_SCOPE_PROCESS 1

#define PTHREAD_MUTEX_DEFAULT 0
#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_RECURSIVE 1

#define PTHREAD_PROCESS_PRIVATE 0
#define PTHREAD_PROCESS_SHARED 1

#define PTHREAD_PRIO_NONE 0
#define PTHREAD_PRIO_INHERIT 1
#define PTHREAD_PRIO_PROTECT 2

#define SCHED_FIFO 0

#define PTHREAD_MUTEX_STALLED 0
#define PTHREAD_MUTEX_ROBUST 1


// Bincows constant thread stack size
#define PTHREAD_STACK_MIN (32 * 1024)

typedef struct {
    int taken;
	
	int type;

	// for recursive locks
	tid_t owner;
	int rec;


    char padding[128 - 8];
} pthread_mutex_t;


typedef unsigned int pthread_cond_t;



typedef struct {int type;} pthread_mutexattr_t;
typedef struct {} pthread_key_t;
typedef struct {} pthread_condattr_t;




int pthread_create(pthread_t* __restrict newthread,
			   const pthread_attr_t* __restrict attr,
			   void* (*__start_routine)(void* ),
			   void* __restrict arg);

void pthread_exit(void* retval) __attribute__((__noreturn__));


int pthread_join(pthread_t th, void** thread_return);


/* Indicate that the thread TH is never to be joined with PTHREAD_JOIN.
   The resources of TH will therefore be freed immediately when it
   terminates, instead of waiting for another thread to perform PTHREAD_JOIN
   on it.  */
int pthread_detach(pthread_t th);


/* Obtain the identifier of the current thread.  */
pthread_t pthread_self(void) __attribute__((__const__));


/* Compare two thread identifiers.  */
int pthread_equal(pthread_t thread1, pthread_t thread2)
  __attribute__((__const__));


int pthread_attr_init(pthread_attr_t* attr);


int pthread_attr_destroy(pthread_attr_t* attr);

int pthread_attr_getdetachstate(const pthread_attr_t* attr,
					int *detachstate);

/* Set detach state attribute.  */
int pthread_attr_setdetachstate(pthread_attr_t* attr,
					int detachstate);


/* Get the size of the guard area created for stack overflow protection.  */
int pthread_attr_getguardsize(const pthread_attr_t* attr,
				      size_t* guardsize);


/* Set the size of the guard area created for stack overflow protection.  */
int pthread_attr_setguardsize(pthread_attr_t* attr,
				      size_t guardsize);


#include <signal.h>

int pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset);


#include <sched.h>


/* Return in *PARAM the scheduling parameters of *ATTR.  */
int pthread_attr_getschedparam(const pthread_attr_t* __restrict attr,
				       struct sched_param *__restrict param);

/* Set scheduling parameters(priority, etc) in *ATTR according to PARAM.  */
int pthread_attr_setschedparam(pthread_attr_t* __restrict attr,
				       const struct sched_param *__restrict
				       param);

/* Return in *POLICY the scheduling policy of *ATTR.  */
int pthread_attr_getschedpolicy(const pthread_attr_t* __restrict
					attr, int *__restrict policy);

/* Set scheduling policy in *ATTR according to POLICY.  */
int pthread_attr_setschedpolicy(pthread_attr_t* attr, int policy);

/* Return in *INHERIT the scheduling inheritance mode of *ATTR.  */
int pthread_attr_getinheritsched(const pthread_attr_t* __restrict
					 attr, int *__restrict inherit);

/* Set scheduling inheritance mode in *ATTR according to INHERIT.  */
int pthread_attr_setinheritsched(pthread_attr_t* attr,
					 int inherit);


/* Return in *SCOPE the scheduling contention scope of *ATTR.  */
int pthread_attr_getscope(const pthread_attr_t* __restrict attr,
				  int *__restrict scope);

/* Set scheduling contention scope in *ATTR according to SCOPE.  */
int pthread_attr_setscope(pthread_attr_t* attr, int scope);


/* Return the currently used minimal stack size.  */
int pthread_attr_getstacksize(const pthread_attr_t* __restrict
				      attr, size_t* __restrict stacksize);

/* Add information about the minimum stack size needed for the thread
   to be started.  This size must never be less than PTHREAD_STACK_MIN
   and must also not exceed the system limits.  */
int pthread_attr_setstacksize(pthread_attr_t* attr,
				      size_t stacksize);

/* Functions for scheduling control.  */

/* Set the scheduling parameters for TARGET_THREAD according to POLICY
   and *PARAM.  */
int pthread_setschedparam(pthread_t target_thread, int policty,
				  const struct sched_param *param);

/* Return in *POLICY and *PARAM the scheduling parameters for TARGET_THREAD. */
int pthread_getschedparam(pthread_t target_thread,
				  int *__restrict policty,
				  struct sched_param *__restrict param);

/* Set the scheduling priority for TARGET_THREAD.  */
int pthread_setschedprio(pthread_t target_thread, int prio);


/* Functions for handling cancellation.

   Note that these functions are explicitly not marked to not throw an
   exception in C++ code.  If cancellation is implemented by unwinding
   this is necessary to have the compiler generate the unwind information.  */

/* Set cancelability state of current thread to STATE, returning old
   state in *OLDSTATE if OLDSTATE is not NULL.  */
int pthread_setcancelstate(int state, int *oldstate);

/* Set cancellation state of current thread to TYPE, returning the old
   type in *OLDTYPE if OLDTYPE is not NULL.  */
int pthread_setcanceltype(int type, int *oldtype);

/* Cancel THREAD immediately or at the next possibility.  */
int pthread_cancel(pthread_t th);

/* Test for pending cancellation for the current thread and terminate
   the thread as per pthread_exit(PTHREAD_CANCELED) if it has been
   cancelled.  */
void pthread_testcancel(void);

/////////////////////////////////////////////////////
////////////////////// MUTEXES /////////////////////
/////////////////////////////////////////////////////
/* Mutex handling.  */

/* Initialize a mutex.  */
int pthread_mutex_init(pthread_mutex_t* mutex,
			       const pthread_mutexattr_t* mutexattr);

/* Destroy a mutex.  */
int pthread_mutex_destroy(pthread_mutex_t* mutex);

/* Try locking a mutex.  */
int pthread_mutex_trylock(pthread_mutex_t* mutex);

/* Lock a mutex.  */
int pthread_mutex_lock(pthread_mutex_t* mutex);

/* Unlock a mutex.  */
int pthread_mutex_unlock(pthread_mutex_t* mutex);


/* Get the priority ceiling of MUTEX.  */
int pthread_mutex_getprioceiling(const pthread_mutex_t* 
					 __restrict mutex,
					 int *__restrict prioceiling);

/* Set the priority ceiling of MUTEX to PRIOCEILING, return old
   priority ceiling value in *OLD_CEILING.  */
int pthread_mutex_setprioceiling(pthread_mutex_t* __restrict mutex,
					 int prioceiling,
					 int *__restrict old_ceiling);

int pthread_mutexattr_getrobust(const pthread_mutexattr_t *attr,
							int *robustness);



int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type);
int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type);


/* Functions for handling mutex attributes.  */

/* Initialize mutex attribute object ATTR with default attributes
  (kind is PTHREAD_MUTEX_TIMED_NP).  */
int pthread_mutexattr_init(pthread_mutexattr_t* attr);

/* Destroy mutex attribute object ATTR.  */
int pthread_mutexattr_destroy(pthread_mutexattr_t* attr);

/* Get the process-shared flag of the mutex attribute ATTR.  */
int pthread_mutexattr_getpshared(const pthread_mutexattr_t* 
					 __restrict attr,
					 int *__restrict pshared);

/* Set the process-shared flag of the mutex attribute ATTR.  */
int pthread_mutexattr_setpshared(pthread_mutexattr_t* attr,
					 int pshared);

/* Return in *PROTOCOL the mutex protocol attribute in *ATTR.  */
int pthread_mutexattr_getprotocol(const pthread_mutexattr_t* 
					  __restrict attr,
					  int *__restrict protocol);

/* Set the mutex protocol attribute in *ATTR to PROTOCOL(either
   PTHREAD_PRIO_NONE, PTHREAD_PRIO_INHERIT, or PTHREAD_PRIO_PROTECT).  */
int pthread_mutexattr_setprotocol(pthread_mutexattr_t* attr,
					  int protocol);

/* Return in *PRIOCEILING the mutex prioceiling attribute in *ATTR.  */
int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t* 
					     __restrict attr,
					     int *__restrict prioceiling);

/* Set the mutex prioceiling attribute in *ATTR to PRIOCEILING.  */
int pthread_mutexattr_setprioceiling(pthread_mutexattr_t* attr,
					     int prioceiling);


/* Set the robustness flag of the mutex attribute ATTR.  */
int pthread_mutexattr_setrobust(pthread_mutexattr_t* attr,
					int robustness);

					
int pthread_mutexattr_getrobust(const pthread_mutexattr_t *attr,
					int *robustness);


#if 0
/* Acquire write lock for RWLOCK.  */
int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock);

/* Try to acquire write lock for RWLOCK.  */
int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock);

#endif


/* Functions for handling conditional variables.  */

/* Initialize condition variable COND using attributes ATTR, or use
   the default values if later is NULL.  */
int pthread_cond_init(pthread_cond_t* __restrict cond,
			      const pthread_condattr_t* __restrict cond_attr);

/* Destroy condition variable COND.  */
int pthread_cond_destroy(pthread_cond_t* cond);

/* Wake up one thread waiting for condition variable COND.  */
int pthread_cond_signal(pthread_cond_t* cond);

/* Wake up all threads waiting for condition variables COND.  */
int pthread_cond_broadcast(pthread_cond_t* cond);

/* Wait for condition variable COND to be signaled or broadcast.
   MUTEX is assumed to be locked before.

   This function is a cancellation point and therefore not marked with
  .  */
int pthread_cond_wait(pthread_cond_t* __restrict cond,
			      pthread_mutex_t* __restrict mutex);


/* Functions for handling condition variable attributes.  */

/* Initialize condition variable attribute ATTR.  */
int pthread_condattr_init(pthread_condattr_t* attr);

/* Destroy condition variable attribute ATTR.  */
int pthread_condattr_destroy(pthread_condattr_t* attr);

/* Get the process-shared flag of the condition variable attribute ATTR.  */
int pthread_condattr_getpshared(const pthread_condattr_t* 
					__restrict attr,
					int *__restrict pshared);

/* Set the process-shared flag of the condition variable attribute ATTR.  */
int pthread_condattr_setpshared(pthread_condattr_t* attr,
					int pshared);

/* Functions for handling thread-specific data.  */

/* Create a key value identifying a location in the thread-specific
   data area.  Each thread maintains a distinct thread-specific data
   area.  DESTR_FUNCTION, if non-NULL, is called with the value
   associated to that key when the key is destroyed.
   DESTR_FUNCTION is not called if the value associated is NULL when
   the key is destroyed.  */
int pthread_key_create(pthread_key_t* key,
			       void(*__destr_function)(void* ));

/* Destroy KEY.  */
int pthread_key_delete(pthread_key_t key);

/* Return current value of the thread-specific data slot identified by KEY.  */
void* pthread_getspecific(pthread_key_t key);

/* Store POINTER in the thread-specific data slot identified by KEY. */
int pthread_setspecific(pthread_key_t key,
				const void* pointer);


#ifdef __cplusplus
}
#endif

#endif//_PTHREAD_H