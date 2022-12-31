#include <sys/events.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <bc_extsc.h>
#include <pthread.h>

#define EXTOUT_FILENO 3

#define SH_PATH "/bin/sh"
#define DE_PATH "/bin/bde"


// if the read_events falls,
// the reader waits until the cond
// is signaled
static int read_events = 1;
static pthread_mutex_t read_events_mutex;
static pthread_cond_t  read_events_cond;


#define OPCODE_PROCESS_EVENTS 1

struct extout_cmd {
    uint32_t opcode;
    uint8_t cmd[32];
};

// extout thread
void* extout_main(void* a) {
    while(1) {
        struct extout_cmd cmd;

        int res = read(4, &cmd, sizeof(cmd));

        if(res != sizeof(cmd)) {
            break;
        }

        switch(cmd.opcode) {
            case OPCODE_PROCESS_EVENTS:
                if(cmd.cmd[0] == 0)
                    read_events = 0;
                else
                    read_events = 1;
                pthread_cond_signal(&read_events_cond);
                break;
            default:
                // unrecognized
                break;
        };
    }

    read_events = 1;
    pthread_cond_signal(&read_events_cond);
    return NULL;
}





void open_stdout_stderr(void) {
    int fd = open("/dev/term", O_RDWR, 0);

    assert(fd == 0);

    int res;


    res = dup2(0, 1); assert(res == 1);
        
    res = dup2(0, 2); assert(res == 2);

    close(0);
}

static volatile int sigchld = 0;

void sigchld_h(int sig) {
    sigchld = 1;
}


void prepare_forkexec(void) {
    // user fd disposition:
    // 0: READ  read pipe end  - stdin   init -> user
    // 1: WRITE /dev/term      - stdout  user -> sys
    // 2: WRITE /dev/term      - stderr  user -> sys
    // 3: WRITE pipe write end - extout  user -> init


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
}

void close_pipes(void) {
    close(0); // stdin
    close(3); // extout write
    close(4); // extout read
    close(6); // stdin write
}


void execute(const char* path) {
    const char* const sh_argv[] = {path, NULL};

    prepare_forkexec();
    // keep fds 0, 1, 2, 3
    forkexec(sh_argv, ~0x0f);
    pthread_create(NULL, NULL, extout_main, NULL);
}



void read_events_loop(void) {
    struct sys_event ev = {0};


    FILE* evfile = fopen(SYS_EVENT_FILE, "r");

    if(!evfile) {
        printf("init: couldn't open system event file\n");
        exit(1);
    }


    while(1) {
        if(!read_events) {
            pthread_mutex_lock(&read_events_mutex);

            while(!read_events) {
                pthread_cond_wait(&read_events_cond, &read_events_mutex);
            }   

            pthread_mutex_unlock(&read_events_mutex);
        }
        
        if(sigchld)
            break;

        int res = read(fileno(evfile), &ev, sizeof(ev));
        
        if(sigchld)
            break;


        if(res != sizeof(ev)) {
            printf("init: broken pipe %d\n", res);
            exit(1);
        }


        int ascii_key = (ev.type == KEYPRESSED) && ev.unix_sequence[0] != 0;


        if(ascii_key) {
            // only keep the unix sequence
            res = write(6, ev.unix_sequence, strlen(ev.unix_sequence));
            if(res <= 0) {
                printf("init: broken pipe %d\n", res);
                exit(1);
            }
        }
    }

    sigchld = 0;


    fclose(evfile);
}


int main(int argc, char** argv) {

    open_stdout_stderr();

    signal(SIGCHLD, sigchld_h);



    pthread_mutex_init(&read_events_mutex, NULL);
    pthread_cond_init(&read_events_cond, NULL);


    while(1) {
        execute(SH_PATH);
        read_events_loop();
        close_pipes();


        execute(DE_PATH);
        read_events_loop();
        close_pipes();

    }
}
