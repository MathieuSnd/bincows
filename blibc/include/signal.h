#ifndef	SIGNAL_H
#define	SIGNAL_H

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
extern sighandler_t signal(int sig, sighandler_t handler);

// same as kill(get_pid(), sig)
#define raise(sig) sigsend(get_pid(), sig)


#define kill(pid, sig) sigsend(pid, sig)


#endif	/* SIGNAL_H */



