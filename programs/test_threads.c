//#include <pthread.h>


#include <bc_extsc.h>
#include <unistd.h>


void thread_entry(const char* print) {

    for(;;) {
        printf("%s!\n", print);

        usleep(500);
    }
}


int main() {
    _thread_create(thread_entry, "thread 1");
    usleep(500);
    _thread_create(thread_entry, "thread 2");
    for(;;) {
        printf("thread 0\n");
        usleep(500);
    }
}
