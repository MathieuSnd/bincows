#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


int fib(int n) {
    if(n <= 1) {
        return n;
    }

    return fib(n-1) + fib(n-2);
}


#define assert(x, y) if(!(x == y)) { \
    printf("assertion failed: %s=%lx instead of %u\n", #x, x, y); \
    exit(1); \
    }





volatile int pid;

void main(int argc, char* argv[]) {
    pid = getpid();

    int i = 30;
    
    while(1) {
        printf("--- pid %u --- fib(%d)=%d\n", pid, i, fib(i));
        //i = i + 1 > 30 ? 30 : i + 1;
    }
}