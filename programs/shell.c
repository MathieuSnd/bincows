#include <unistd.h>

#include <string.h>
#include <alloc.h>

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


#define VERSION "0.1"

const char* version_string = 
        "Welcome in the Bincows Shell!\n"
        "Version: " VERSION "\n"
        "Copyright (c) 2020-2021 by Mathieu Serandour\n"
        "see https://git.rezel.net/jambonoeuf/Bincows/\n"
        "type help to list commands\n\n"
;

/*
int print_input_end() {
    
    return 0;
}
*/

void init_stream(void) {
    int __stdin  = open("/dev/ps2kb", 0,0);
    int __stdout = open("/dev/term", 0,0);
    int __stderr = open("/dev/term", 0,0);

    if(__stdin  != STDIN_FILENO)
        *(int*)0 = 0;
    if(__stdout != STDOUT_FILENO)
        *(int*)2 = 0;
    if(__stderr != STDERR_FILENO)
        *(int*)3 = 0;
}


void print(const char* str) {
    write(STDOUT_FILENO, str, strlen(str));
}


void print_cow(void) {
    int file = open("/boot/boot_message.txt", 0,0);
    if(file < 0) {
        print("/boot/cow not found\n");
        return;
    }

    
    size_t file_size = lseek(file, 0, SEEK_END);

    lseek(file, 0, SEEK_SET);

    char* buf = malloc(file_size);

    if(!buf) {
        print("malloc failed\n");
        return;
    }

    size_t n = read(file, buf, file_size);

    write(STDOUT_FILENO, buf, n);


    free(buf);

    close(file);
}

int list_dirs(void) {
    int fd = open("/", 0, 0);
    if(fd < 0) {
        print("fopen failed\n");
        return -1;
    }

    size_t size = lseek(fd, 0, SEEK_END);

    lseek(fd, 0, SEEK_SET);

    
    char* buf = malloc(size);

    if(!buf) {
        print("malloc failed\n");
        return -1;
    }

    size_t n = read(fd, buf, size);

typedef uint64_t ino_t;
typedef struct dirent {
        struct {
            ino_t         ino;       /* Inode number */
            size_t        file_size; /* Length of this record */
            unsigned char type;      /* Type of file; not supported
                                        by all filesystem types */
        };
    char name[256];   /* Null-terminated filename */
} dirent_t;


    struct dirent* dirs = (struct dirent *)buf;


    for(int i =  0; i < n / sizeof(struct dirent); i++) {
        print(dirs[i].name);
        print("'\n'");
    }


    free(buf);
}


int _start() {
    init_stream();

    print_cow();
    list_dirs();

    write(STDOUT_FILENO, version_string, strlen(version_string));

    const char* prompt = "> _\b";

    write(STDOUT_FILENO, prompt, strlen(prompt));

    while(1) {
        char ch;

        if(read(STDIN_FILENO, &ch, 1) <= 0)
            break;

        switch(ch) {
            default:
                write(STDOUT_FILENO, &ch, 1);
                write(STDOUT_FILENO, "_\b", 2);
                break;
            case '\b':
                write(STDOUT_FILENO, " \b\b_\b", 5);
                break;
        }
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);


    for(;;);
}
