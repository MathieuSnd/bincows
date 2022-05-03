#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char** __environ = NULL;

extern int main(int argc, char** argv);

// defined in stdio.c
extern void stio_init(void);


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


    stio_init();


    int ret = main(argc, argv);

    exit(ret);
}