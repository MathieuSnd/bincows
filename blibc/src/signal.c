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
