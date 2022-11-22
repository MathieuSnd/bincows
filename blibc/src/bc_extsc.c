#include "bc_extsc.h"

// for syscall(...)
#include "unistd.h"

#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sprintf.h>

// for system call numbers
#include "../../kernel/int/syscall_interface.h"

int sigsetup(struct sigsetup state) {
    struct sc_sigsetup_args args = {
        .ign_mask       = state.ign_mask,
        .blk_mask       = state.blk_mask,
        .signal_handler = state.handler,
    };

    return syscall(SC_SIGSETUP, &args, sizeof(args));
}


void sigreturn(void) {
    syscall(SC_SIGRETURN, NULL, 0);

    printf("sigreturn: unreachable code reached\n");
    for(;;);
}


int sigsend(int pid, int sig) {
    struct sc_sigkill_args args = {
        .pid    = pid,
        .signal = sig,
    };
    return syscall(SC_SIGKILL, &args, sizeof(args));
}

int _thread_create(void* entry, void* arg) {
    struct sc_thread_create_args args = {
        .entry = entry,
        .argument = arg,
    };
    return syscall(SC_THREAD_CREATE, &args, sizeof(args));
}

void __attribute__((noreturn)) _thread_exit(void) {
    syscall(SC_THREAD_EXIT, NULL, 0);
    __builtin_unreachable();
}



