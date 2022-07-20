#include "bc_extsc.h"

// for syscall(...)
#include "unistd.h"

#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sprintf.h>

// for system call numbers
#include "../../kernel/int/syscall_interface.h"



int sigsetup(signal_end_fun_t sigend, sighandler_t* handler_table) {
    struct sc_sigsetup_args args = {
        .handler_table = handler_table,
        .signal_end = sigend,
    };
    return syscall(SC_SIGSETUP, &args, sizeof(args));
}


void sigreturn(void) {
    syscall(SC_SIGRETURN, NULL, 0);

    printf("sigreturn: should never reach this point\n");
    for(;;);
}


int sigsend(int pid, int sig) {
    struct sc_sigkill_args args = {
        .pid    = pid,
        .signal = sig,
    };
    return syscall(SC_SIGKILL, &args, sizeof(args));
}