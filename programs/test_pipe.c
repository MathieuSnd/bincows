#include <stdint.h>
#include <stdio.h>
#include <unistd.h>


int main(void)
{
    int fds[2];
    pipe(fds);

    printf("fds[0] = %d\n", fds[0]);
    printf("fds[1] = %d\n", fds[1]);

    // write to pipe
    write(fds[1], "hello", 5);
    write(fds[1], "world", 5);
    write(fds[1], "!", 1);

    // read from pipe
    char buf[11];
    int r = read(fds[0], buf, 10);
    buf[r] = '\0';
    printf("%s\n", buf);


    // write to pipe
    write(fds[1], "hello", 5);
    write(fds[1], "world", 5);
    write(fds[1], "!", 1);
    write(fds[1], "", 1);


    // read from pipe
    buf[11];
    r = read(fds[0], buf, 10);
    buf[r] = '\0';
    printf("%s\n", buf);


    // read from pipe
    buf[11];
    r = read(fds[0], buf, 10);
    buf[r] = '\0';
    printf("%s\n", buf);

    close(fds[0]);
    close(fds[1]);



    return 0;
}