#include <unistd.h>
#include <bc_extsc.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signums.h>

char** __environ = NULL;

extern int main(int argc, char** argv);

// defined in stdio.c
extern void stio_init(void);





static void __sig_terminate(int sig) {
    (void) sig;
    _exit(1);
}

static sighandler_t __sig_handler_table[MAX_SIGNALS] = {NULL};

/*
static void __sig_ignore(int sig) {
    //printf("process %d: ignoring signal %d\n", getpid(),    sig); 
}
*/

#define __sig_ignore NULL

static sighandler_t __sig_dfl_table[] = {
__sig_terminate, __sig_terminate, __sig_terminate, __sig_terminate,
__sig_terminate, __sig_terminate, __sig_terminate, __sig_terminate,
__sig_terminate, __sig_terminate, __sig_terminate, __sig_terminate,
__sig_terminate, __sig_terminate, __sig_terminate, __sig_terminate,
__sig_terminate, __sig_ignore,    __sig_ignore,    __sig_terminate,
__sig_terminate, __sig_terminate, __sig_terminate, __sig_ignore,
__sig_terminate, __sig_terminate, __sig_terminate, __sig_terminate,
__sig_ignore,    __sig_ignore,    __sig_terminate, __sig_terminate,
};




static void init_signals(void) {
    memcpy(
        __sig_handler_table,
        __sig_dfl_table,
        sizeof(__sig_handler_table)
    );

    sigsetup(sigreturn, __sig_handler_table);
}


static void init_argv(int argc, void* args, char** argv) {

    char* p = args;

    int i = 0;
    
    while(*p) {
        if(i >= argc)
            break;
        argv[i++] = p;
        while(*p) p++;
        p++;
    }

    if(argc != i) {
        printf("bad argc\n");
        exit(1);
    }
}

void __attribute__((noreturn)) 
_start(int argc, char* args, int envrionc, char* input_environ) {
    // compute argv and argc
    char** argv = alloca(argc * sizeof(char*));

    init_argv(argc, args, argv);

    // compute environ
    char** environ;

    __environ = environ = malloc(envrionc);

    char** p = environ;

    for(int i = 0; i < envrionc; i++) {
        *p++ = strdup(input_environ);
        input_environ += strlen(input_environ) + 1;
    }

    *p = 0;

    // init signals

    init_signals();


    stio_init();


    int ret = main(argc, argv);

    exit(ret);
}