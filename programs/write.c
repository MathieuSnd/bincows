#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


struct FILE {
    int    fd;
    int    flags;
    int    mode;
    int    ungetc;
    int    error;
    int    eof;
    int    pos;
    size_t size;
    size_t bufsize;
    char*  buffer;
    size_t buf_off;

    // 0 if the buffer was not read/written
    // 1 if the buffer was read to 
    // 2 if the buffer was written to
    int buff_state;
};


int main(int argc, char** argv) {
    FILE* f = fopen("/home/elyo", "a");
    if(!f) {
        printf("couldn't open file\n");
        return 1;
    }
    
    for(unsigned i = 0; i < 1; i++) {
        write(f->fd, "hello world\n", strlen("hello world\n"));
        //write(f->fd, "hello world\n", 1);
        //fprintf(f, "h");
        //sleep(3000);
    }

    fclose(f);
    return 0;
}
