#include <sys/events.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <bc_extsc.h>
#include <pthread.h>

#define EXTOUT_RD 4

#define SH_CMD "/bin/bde"


// extout thread
void* extout_main(void* a) {
    while(1) {
        uint32_t cmd;

        int res = read(EXTOUT_RD, &cmd, 4);

        if(!res) {
            return NULL;
        }

        // maybe do something
    }
}




void open_stdout_stderr(void) {
    int fd = open("/dev/term", O_RDWR, 0);

    assert(fd == 0);

    int res;


    res = dup2(0, 1); assert(res == 1);
        
    res = dup2(0, 2); assert(res == 2);

    close(0);
}


int main(int argc, char** argv) {

    open_stdout_stderr();

    // user fd disposition:
    // 0: READ  read pipe end  - stdin   init -> user
    // 1: WRITE /dev/term      - stdout  user -> sys
    // 2: WRITE /dev/term      - stderr  user -> sys
    // 3. WRITE pipe write end - extout  user -> init


    int pipe_stdin_fds[2];
    int pipe_extout_fds[2];
    int res;
    
    res = pipe(pipe_stdin_fds);     assert(!res);
    res = pipe(pipe_extout_fds);    assert(!res);

    // here, fds are:
    // 0 - stdin  (read)
    // 1 - stdout (write)
    // 2 - stderr (write)
    // 3 - stdin  (write)
    // 4 - extout (read)
    // 5 - extout (write)

    assert(pipe_stdin_fds[0] == 0);
    assert(pipe_stdin_fds[1] == 3);


    // move 3 -> 6
    // move 5 -> 3

    res = dup2(3, 6);  assert(res = 6);
    res = dup2(5, 3);  assert(res = 3);
    res = close(5);    assert(!res);


    pipe_stdin_fds[1] = 6;



    const char* const sh_argv[] = {SH_CMD, NULL};

    // keep fds 0, 1, 2, 3
    forkexec(sh_argv, ~0x0f);

    FILE* evfile = fopen(SYS_EVENT_FILE, "r");

    if(!evfile) {
        printf("term: couldn't open system event file\n");
        exit(1);
    }
    close(3); // extout write
    //close(0); // stdin read

    //pthread_create(NULL, NULL, extout_main, NULL);
    struct sys_event ev = {0};
    while(1) {
        res = read(fileno(evfile), &ev, sizeof(ev));
        if(res != sizeof(ev)) {
            break;
        }

        int ascii_key = (ev.type == KEYPRESSED) && ev.unix_sequence[0] != 0;


        if(ascii_key) {
            // only keep the unix sequence
            res = write(6, ev.unix_sequence, strlen(ev.unix_sequence));
            if(res <= 0) {
                printf("init: broken pipe %d\n", res);
                break;
            }
        }
    }

    close(0); // stdin read
    close(1); // stdout write
    close(2); // stderr write
    close(4); // extout read
    close(6); // stdin write
    fclose(evfile);
}
