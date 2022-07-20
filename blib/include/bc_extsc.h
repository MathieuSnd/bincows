#ifndef BC_EXTSC_H
#define BC_EXTSC_H

/**
 * This file describes the Bincows extended system calls.
 */



#ifndef SIG_HANDLER_T

#define SIG_HANDLER_T
typedef void (*sighandler_t)(int);


typedef void (*signal_end_fun_t)(void) __attribute__((noreturn));
#endif

#ifndef MAX_SIGNALS
#define MAX_SIGNALS 32
#endif

/**
 * setup the signal handlers and the signal end function.
 * this should normally be called by the crt0 before calling
 * any program. It can though be changed at any time
 * 
 */
extern int sigsetup(signal_end_fun_t sigend, sighandler_t* handler_table);

/**
 * return from a signal handler.
 * 
 * this system call should be used by the sigend function to 
 * return from a signal handler.
 * 
 */
extern void sigreturn(void) __attribute__((noreturn));


/**
 * send a signal to a process.
 * 0 on success, -1 on error.
 */
extern int sigsend(int sig, int pid);



#endif // BC_EXTSC_H
