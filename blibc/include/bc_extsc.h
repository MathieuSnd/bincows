#ifndef BC_EXTSC_H
#define BC_EXTSC_H

/**
 * This file describes the Bincows extended system calls.
 */


#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifndef SIG_HANDLER_T

#define SIG_HANDLER_T
typedef void (*sighandler_t)(int);


typedef void (*signal_end_fun_t)(void) __attribute__((noreturn));
#endif

#define MAX_SIGNALS 32
#define MAX_FDS 32


#ifndef SIGMASK_T
#define SIGMASK_T
typedef uint32_t sigmask_t;
#endif//SIGMASK_T

struct sigsetup {
    sighandler_t handler;
    sigmask_t ign_mask;
    sigmask_t blk_mask;
    signal_end_fun_t sigend;
};

/**
 * setup the signal handlers and the signal end function.
 * this should normally be called by the crt0 before calling
 * any program. It can though be changed at any time
 * 
 */
int sigsetup(struct sigsetup state);

/**
 * return from a signal handler.
 * 
 * this system call should be used by the sigend function to 
 * return from a signal handler.
 * 
 */
void sigreturn(void) __attribute__((noreturn));


/**
 * send a signal to a process.
 * 0 on success, -1 on error.
 */
int sigsend(int sig, int pid);


typedef uint32_t fd_mask_t;

#ifndef __cplusplus
_Static_assert(MAX_SIGNALS % 8 == 0, "MAX_SIGNALS must be a multiple of 8");
#else
static_assert(MAX_SIGNALS % 8 == 0, "MAX_SIGNALS must be a multiple of 8");
#endif

/** 
 * executes a new task with the given argv.
 * cmdline should be a null terminated array of strings.
 * 
 * mask is a bitmask of file descriptors that are not
 * to inherit to the new process.
*/
int forkexec(char const * const cmdline[], fd_mask_t mask);



/**
 * create a thread that starts immediatelly,
 * with a given entry and argument.
 * entry foramt:
 * 
 * void __attribute__((noreturn)) entry(void* arg)
 * 
 * the function must call thread_exit
 * 
 */
int _thread_create(void* entry, void* arg);


/**
 * this function is the only way to exit from a thread.
 */
void __attribute__((noreturn)) _thread_exit(void);

// return the tid of the current thread
int _get_tid(void);


int futex_wait(void* addr, uint32_t value, uint64_t timeout);
int futex_wake(void* addr, int num);

#ifdef __cplusplus
}
#endif


#endif // BC_EXTSC_H
