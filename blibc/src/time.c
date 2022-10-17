#include <time.h>
#include <unistd.h>

#include <../../kernel/int/syscall_interface.h>


time_t time(time_t *t) {
    time_t r = (time_t)syscall(SC_CLOCK, NULL, 0);

    if(t)
        *t = r;

    printf("time() = %d\n", r);
    return r;
}
