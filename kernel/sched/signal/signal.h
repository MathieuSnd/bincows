#pragma once

#define SIG_IGN ((sighandler_t) 0)

#define SIG_HANDLER_T
// can be either SIG_IGN or a user-space function
typedef void (*sighandler_t)(int);


#define SIGHUP     1 
#define SIGINT     2 
#define SIGQUIT    3 
#define SIGILL     4 
#define SIGTRAP    5 
#define SIGABRT    6 
#define SIGIOT     6 
#define SIGBUS     7 
#define SIGFPE     8 
#define SIGKILL    9 
#define SIGUSR1    10
#define SIGSEGV    11
#define SIGUSR2    12
#define SIGPIPE    13
#define SIGALRM    14
#define SIGTERM    15
#define SIGSTKFLT  16
#define SIGCHLD    17
#define SIGCONT    18
#define SIGSTOP    19
#define SIGTSTP    20
#define SIGTTIN    21
#define SIGTTOU    22
#define SIGURG     23
#define SIGXCPU    24
#define SIGXFSZ    25
#define SIGVTALRM  26
#define SIGPROF    27
#define SIGWINCH   28
#define SIGIO      29
#define SIGPWR     30
#define SIGSYS     31

#define MAX_SIGNALS 32

#define NOSIG (-1)
/*


void register_signal(int signal, void (*handler)(int));
void signal_init();

struct process_signal_handling {
    unsigned mask;
    unsigned pending;
    void (*handlers[32])(int);
};

void signal_process(int pid, int signal);


*/