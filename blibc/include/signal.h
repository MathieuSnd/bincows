#ifndef	SIGNAL_H
#define	SIGNAL_H


#ifdef __cplusplus
extern "C" {
#endif

#include <signums.h>

// for pid_t
#include <unistd.h>

// for sigsend
#include <bc_extsc.h>

typedef void (*sighandler_t)(int);


// for signal(...)
#define SIG_ERR ((sighandler_t)-1)
#define SIG_IGN ((sighandler_t)0)
#define SIG_DFL ((sighandler_t)1)



/* Set the handler for the signal SIG to HANDLER, returning the old
   handler, or SIG_ERR on error.
*/
sighandler_t signal(int sig, sighandler_t handler);

// same as kill(get_pid(), sig)
#define raise(sig) sigsend(getpid(), sig)


#define kill(pid, sig) sigsend(pid, sig)




void psignal(int sig, const char *s);
//void psiginfo(const siginfo_t *pinfo, const char *s);


typedef uint32_t sigset_t;


/* Clear all signals from SET.  */
int sigemptyset (sigset_t *set);

/* Set all signals in SET.  */
int sigfillset (sigset_t *set);

/* Add SIGNO to SET.  */
int sigaddset (sigset_t *set, int signo);

/* Remove SIGNO from SET.  */
int sigdelset (sigset_t *set, int signo);

/* Return 1 if SIGNO is in SET, 0 if not.  */
int sigismember (const sigset_t *set, int signo);


int sigprocmask(int how, const sigset_t *__restrict__ set,
                           sigset_t *__restrict__ oldset);

#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2


#ifdef __cplusplus
}
#endif

#endif	/* SIGNAL_H */
