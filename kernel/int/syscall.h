#pragma once
#include <stddef.h>
#include <stdint.h>

typedef unsigned fd_t;


/**
 * @brief write msr to enable
 * syscalls
 * 
 */
void syscall_init(void);

// bincows syscalls

#define SC_SLEEP         1 
#define SC_CLOCK         2 
#define SC_EXIT          3 
#define SC_OPEN          4 
#define SC_CLOSE         5 
#define SC_READ          6 
#define SC_WRITE         7 
#define SC_SEEK          8 
#define SC_DUP           9 
#define SC_CREATE_THREAD 10
#define SC_JOIN_THREAD   11
#define SC_EXIT_THREAD   12
#define SC_SBRK          13
#define SC_FORK          14
#define SC_EXEC          15
#define SC_CHDIR         16
#define SC_GETCWD        17
#define SC_GETPID        18
#define SC_GETPPID       19
#define SC_END           20



typedef enum open_mode {
    MODE_READ = 0,
    MODE_WRITE = 1,
    MODE_READ_WRITE = 2,
} open_mode_t;


typedef enum open_flags {
    O_RDONLY    = 0,
    O_WRONLY    = 1,
    O_RDWR      = 2,
    O_CREAT     = 4,
    O_EXCL      = 8,
    O_TRUNC     = 16,
    O_APPEND    = 32,
    O_NONBLOCK  = 64,
    O_DIRECTORY = 128,
    O_NOFOLLOW  = 256,
    O_DIRECT    = 512,
    O_NOATIME   = 1024,
    O_CLOEXEC   = 2048,
} open_flags_t; 


struct sc_open_args {
    const char* path;
    size_t path_len;

    open_flags_t flags;
    open_mode_t mode;

    char chs[3];
};


struct sc_seek_args {
    fd_t fd;
    int64_t offset;
    int whence;
};


struct sc_read_args {
    fd_t fd;
    void* buf;
    size_t count;
};

struct sc_write_args {
    fd_t fd;
    const void* buf;
    size_t count;
};


struct sc_exec_args {

    // args contains 0 terminated args
    // total size: args_sz 
    const char* args;
    size_t args_sz;

    // args: argv[0]
    // args+strlen(argv[0])+1: argv[1]
    // ...

    // if 0, the syscall will 
    // behave like the unix one
    int new_process;
};


struct sc_chdir_args {
    const char* path;
    size_t path_len;
};

struct sc_getcwd_args {
    char* buf;
    size_t buf_sz;
};

struct sc_dup_args {
    fd_t fd;
    fd_t fd2; 
    // if new == -1, this operation 
    // performs dup(fd)
    // else, it performs dup2(fd, fd2)
};