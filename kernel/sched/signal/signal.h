#pragma once

#define SIG_IGN ((sighandler_t) 0)

#define SIG_HANDLER_T
// can be either SIG_IGN or a user-space function
typedef void (*sighandler_t)(int);

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