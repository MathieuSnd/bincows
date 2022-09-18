//#include <pthread.h>


#include <bc_extsc.h>
#include <unistd.h>
#include <signal.h>


void thread_entry(const char* print) {

    for(;;) {
        printf("%s!\n", print);

        usleep(500);
    }
}


void handler(int n) {
    printf("signal %u ----------------------------------------\n", n);
}


int main() {
    signal(SIGINT, handler);

    _thread_create(thread_entry, "thread 1");
    usleep(0);
    _thread_create(thread_entry, "thread 2");
    for(int i = 0; i < 10; i++) {
        printf("thread 0\n");
        usleep(500);
    }
}
