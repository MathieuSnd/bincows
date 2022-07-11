/*

#pragma once

void register_signal(int signal, void (*handler)(int));
void signal_init();

struct process_signal_handling {
    unsigned mask;
    unsigned pending;
    void (*handlers[32])(int);
};

void signal_process(int pid, int signal);


*/