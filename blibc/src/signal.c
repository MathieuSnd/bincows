#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <assert.h>

static void sig_terminate(int sig) {
    (void) sig;
    _exit(1);
}

static void sig_ignore(int sig) {
    printf("fatal signal error: unreachable (sig %d, pid %i)\n", sig, getpid());
    _exit(1);
}

static sighandler_t sig_handler_table[MAX_SIGNALS] = {NULL};


static void signal_handler(int sig) {
    assert(sig < MAX_SIGNALS);
    sig_handler_table[sig](sig);
    sigreturn();
} 


typedef uint32_t sigmask_t;

// 00: ignore
// 10: blocked
// 01: call function

// for signals that terminate by default,
// the terminason is done by calling the sig_terminate function
//  need C23 to use 0011'0000'1000'0110'0000'0000'0000'0000...
#define DEFL_IGNMASK 0b00110000100001100000000000000000


static sigmask_t ign_mask = DEFL_IGNMASK;

static sigmask_t blk_mask = 0; // no blocked sig


/**
 * unreachable for SIG_IGN:
 * the kernel shouldn't make a call in this case
 */
static sighandler_t sig_dfl_table[] = {
    sig_terminate, sig_terminate, sig_terminate, sig_terminate,
    sig_terminate, sig_terminate, sig_terminate, sig_terminate,
    sig_terminate, sig_terminate, sig_terminate, sig_terminate,
    sig_terminate, sig_terminate, sig_terminate, sig_terminate,
    sig_terminate, sig_ignore,    sig_ignore,    sig_terminate,
    sig_terminate, sig_terminate, sig_terminate, sig_ignore,
    sig_terminate, sig_terminate, sig_terminate, sig_terminate,
    sig_ignore   , sig_ignore   , sig_terminate, sig_terminate,
};


static void sig_update(void) {
    struct sigsetup state = {
        .handler = signal_handler,
        .blk_mask = blk_mask,
        .ign_mask = ign_mask,
        .sigend   = sigreturn,
    };

    sigsetup(state);
}



sighandler_t signal(int sig, sighandler_t handler) {
    if(sig < 0 || sig >= MAX_SIGNALS)
        return SIG_ERR;


    sighandler_t old;
    int old_ign = (ign_mask >> sig) & 1;

    if(old_ign)
        old = SIG_IGN;
    else
        old = sig_handler_table[sig];

    if(old == sig_dfl_table[sig])
        old = SIG_DFL;

    if(handler != old) {
        int ign = 0;
        if(handler == SIG_DFL) {
            handler = sig_dfl_table[sig];
            ign = (DEFL_IGNMASK >> sig) & 1;
        }
        else if(handler == SIG_IGN) {
            ign = 1;
        }
        else
            sig_handler_table[sig] = handler;

        if(ign != old_ign) {
            ign_mask ^= (1 << sig);
            sig_update();
        }
    }


    return old;
}


void __signals_init(void) {
    memcpy(
        sig_handler_table,
        sig_dfl_table,
        sizeof(sig_handler_table)
    );
    sig_update();
}


int sigprocmask(int how, const sigset_t *restrict set,
                           sigset_t *restrict oldset) 
{
    if(oldset)
        *oldset = 0;
    if(set == NULL)
        return 0;
    switch(how) {
        case SIG_BLOCK:
            // @todo block mask
            break;
        case SIG_UNBLOCK:
            // @todo block mask
            break;
        case SIG_SETMASK:
            // @todo block mask
            break;
        default:
            return -1;
    }

    return 0;
}




void psignal(int sig, const char *s) {
    const char* signame;

    switch(sig) {
        case SIGBUS   : signame = "SIGBUS";   break;
        case SIGHUP   : signame = "SIGHUP";   break;case SIGINT   : signame = "SIGINT";break;
        case SIGQUIT  : signame = "SIGQUIT";  break;case SIGILL   : signame = "SIGILL";break;
        case SIGTRAP  : signame = "SIGTRAP";  break;case SIGABRT  : signame = "SIGABRT";break;
        case SIGFPE   : signame = "SIGFPE";   break;case SIGKILL  : signame = "SIGKILL";break;
        case SIGUSR1  : signame = "SIGUSR1";  break;case SIGSEGV  : signame = "SIGSEGV";break;
        case SIGPIPE  : signame = "SIGPIPE";  break;case SIGALRM  : signame = "SIGALRM";break;
        case SIGTERM  : signame = "SIGTERM";  break;case SIGSTKFLT: signame = "SIGSTKFLT";break;
        case SIGCHLD  : signame = "SIGCHLD";  break;case SIGCONT  : signame = "SIGCONT";break;
        case SIGSTOP  : signame = "SIGSTOP";  break;case SIGTSTP  : signame = "SIGTSTP";break;
        case SIGTTIN  : signame = "SIGTTIN";  break;case SIGUSR2  : signame = "SIGUSR2";break;
        case SIGTTOU  : signame = "SIGTTOU";  break;case SIGURG   : signame = "SIGURG";break;
        case SIGXCPU  : signame = "SIGXCPU";  break;case SIGXFSZ  : signame = "SIGXFSZ";break;
        case SIGVTALRM: signame = "SIGVTALRM";break;case SIGPROF  : signame = "SIGPROF";break;
        case SIGWINCH : signame = "SIGWINCH"; break;case SIGIO    : signame = "SIGIO";break;
        case SIGPWR   : signame = "SIGPWR";   break;case SIGSYS   : signame = "SIGSYS";break;
        default: signame = "invalid signal number";
    }

    if(s)
        fprintf(stderr, "%s: %s\n", s, signame);
    else
        fprintf(stderr, "%s\n", signame);
}


int sigemptyset (sigset_t *set) {
    return *set = 0;
}

int sigfillset (sigset_t *set) {
    *set = 0xffffffff;
    return 0;
}

int sigaddset (sigset_t *set, int signo) {
    *set |= 1 << signo;
    return 0;
}

/* Remove SIGNO from SET.  */
int sigdelset (sigset_t *set, int signo) {
    *set &= ~(1 << signo);
    return 0;
}

/* Return 1 if SIGNO is in SET, 0 if not.  */
int sigismember (const sigset_t *set, int signo) {
    return (*set & (1 << signo)) ? 1 : 0;
}
