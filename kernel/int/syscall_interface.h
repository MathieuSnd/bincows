#pragma once


#include <stddef.h>
#include <stdint.h>


typedef unsigned fd_t;


// bincows syscalls

#define SC_SLEEP         1 
#define SC_CLOCK         2 
#define SC_EXIT          3 
#define SC_OPEN          4 
#define SC_CLOSE         5 
#define SC_READ          6 
#define SC_WRITE         7 
#define SC_TRUNCATE      8
#define SC_SEEK          9
#define SC_ACCESS        10
#define SC_DUP           11
#define SC_PIPE          12
#define SC_THREAD_CREATE 13
#define SC_THREAD_JOIN   14
#define SC_THREAD_EXIT   15
#define SC_SBRK          16
#define SC_FORK          17
#define SC_EXEC          18
#define SC_CHDIR         19
#define SC_GETCWD        20
#define SC_GETPID        21
#define SC_GETPPID       22
#define SC_SIGSETUP      23
#define SC_SIGRETURN     24
#define SC_SIGKILL       25
#define SC_SIGPAUSE      26
#define SC_END           27



typedef enum open_mode {
    MODE_READ = 0,
    MODE_WRITE = 1,
    MODE_READ_WRITE = 2,
} open_mode_t;


struct sc_open_args {
    const char* path;
    size_t path_len;

    open_flags_t flags;
    open_mode_t mode;

    char chs[3];
};

struct sc_access_args {
    const char* path;
    size_t path_len;

    int type;
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

struct sc_truncate_args {
    fd_t fd;
    size_t size;
};


struct sc_exec_args {

    // args contains 0 terminated args
    // total size: args_sz 
    const char* args;
    size_t args_sz;

    // args: argv[0]
    // args+strlen(argv[0])+1: argv[1]
    // ...


    // env contains 0 terminated env
    // total size: env_sz
    const char* env;
    size_t env_sz;

    // same as args
    

    // if 0, the syscall will 
    // behave like the unix one
    int new_process;

    // bit i = 1 means that FD i will not
    // inherit from the parent process
    uint32_t fd_mask;
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



#ifndef SIG_HANDLER_T

#define SIG_HANDLER_T
typedef void (*sighandler_t)(int);

#endif

#ifndef SIGMASK_T
#define SIGMASK_T
typedef uint32_t sigmask_t;
#endif//SIGMASK_T

struct sc_sigsetup_args {
    void (*signal_handler)(int);

    sigmask_t ign_mask;
    sigmask_t blk_mask;
};

struct sc_sigkill_args {
    int pid;
    int signal;
};



struct sc_thread_create_args {
    void* entry;
    void* argument;
};

