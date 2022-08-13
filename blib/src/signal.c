#include <signal.h>
#include <string.h>




static void sig_terminate(int sig) {
    (void) sig;
    _exit(1);
}

static sighandler_t sig_handler_table[MAX_SIGNALS] = {NULL};


#define sig_ignore NULL



static sighandler_t sig_dfl_table[] = {
sig_terminate, sig_terminate, sig_terminate, sig_terminate,
sig_terminate, sig_terminate, sig_terminate, sig_terminate,
sig_terminate, sig_terminate, sig_terminate, sig_terminate,
sig_terminate, sig_terminate, sig_terminate, sig_terminate,
sig_terminate, sig_ignore,    sig_ignore,    sig_terminate,
sig_terminate, sig_terminate, sig_terminate, sig_ignore,
sig_terminate, sig_terminate, sig_terminate, sig_terminate,
sig_ignore,    sig_ignore,    sig_terminate, sig_terminate,
};



sighandler_t signal(int sig, sighandler_t handler) {
    if(sig < 0 || sig >= MAX_SIGNALS)
        return SIG_ERR;

    printf("signal %d, %lx\n", sig, handler);
    
    sighandler_t old = sig_handler_table[sig];

    if(old == sig_dfl_table[sig])
        old = SIG_DFL;
    else if(old == sig_ignore)
        old = SIG_IGN;


    if(handler == SIG_DFL)
        handler = sig_dfl_table[sig];
    else if(handler == SIG_IGN)
        handler = sig_ignore;
    else
        sig_handler_table[sig] = handler;


    return old;
}




void __signals_init(void) {
    memcpy(
        sig_handler_table,
        sig_dfl_table,
        sizeof(sig_handler_table)
    );

    sigsetup(sigreturn, sig_handler_table);
}

