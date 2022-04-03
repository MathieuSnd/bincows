#include <unistd.h>

int x = 8;
const char* rodata = " zefgrbefvzegr ";

int g = 0;

int fibo(int v) {
    if(v < 2)
        return v;
    else
        return fibo(v - 1) + fibo(v - 2);
}

int f(int x) {
    return x+1;
}


const char* string = "hello world from userspace!\n";

int _start() {
    
    int fd = open("/dev/term", 0,0);

    if(fd == -1)
        *(int*)0 = 0;

    write(fd, string, strlen(string));
    write(fd, string, strlen(string));

    close(fd);


    for(;;);
}