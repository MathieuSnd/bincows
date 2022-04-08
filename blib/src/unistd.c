#include <stdarg.h>
#include <alloc.h>
#include "unistd.h"
#include "string.h"

// for system call numbers
#include "../../kernel/int/syscall.h"



static void* brk_addr = NULL;

/*
 * implement syscall wrappers 
 */


int usleep (uint64_t __useconds) {
    syscall(SC_SLEEP, &__useconds, sizeof(__useconds));
}


int brk (void* addr) {
    if(brk_addr == NULL) {
        // fetch  brk
        brk_addr = sbrk(0);
    }

    void* ret = sbrk((uint64_t)addr - (uint64_t)brk_addr);


    return ret == (void*)-1 ? -1 : 0;
}


void *sbrk (uint64_t delta) {
    void* ret = (void*)syscall(SC_SBRK, &delta, sizeof(delta));
    brk_addr += delta;
    
    return ret;
}


void exit (int status) {
    syscall(SC_EXIT, &status, sizeof(status));
}


int open (const char *pathname, int flags, mode_t _mode) {

    open_mode_t mode = (open_mode_t)_mode;
    struct sc_open_args args = {
        .path = pathname,
        .path_len = strlen(pathname),
        .flags = flags,
        .mode = mode,
    };

    return syscall(SC_OPEN, &args, sizeof(args));
}

int close (int fd) {
    return syscall(SC_CLOSE, &fd, sizeof(fd));
}


off_t lseek (int fd, int64_t offset, int whence) {
    struct sc_seek_args args = {
        .fd = fd,
        .offset = offset,
        .whence = whence,
    };

    return syscall(SC_SEEK, &args, sizeof(args));
}

size_t read (int fd, void *buf, size_t count) {
    struct sc_read_args args = {
        .fd = fd,
        .buf = buf,
        .count = count,
    };

    return syscall(SC_READ, &args, sizeof(args));
}


size_t write (int fd, const void *buf, size_t count) {
    struct sc_write_args args = {
        .fd = fd,
        .buf = buf,
        .count = count,
    };

    return syscall(SC_WRITE, &args, sizeof(args));
}


size_t pread (int fd, void* buf, size_t nbytes, off_t offset) {

    off_t off = lseek(fd, 0, SEEK_CUR);

    if(lseek(fd, offset, SEEK_SET) == -1) {
        return -1;
    }

    size_t rd = read(fd, buf, nbytes);

    lseek(fd, off, SEEK_SET);

    return rd;
}


/* Write N bytes of BUF to FD at the given position OFFSET without
   changing the file pointer.  Return the number written, or -1.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
size_t pwrite (int fd, const void* buf, size_t n, off_t offset);


int execv (const char* path, char *const argv[]) {
    if(strcmp(path, argv[0]) != 0) {
        return -1;
    };

    size_t size = 0;

    for(int i = 0; argv[i] != NULL; i++) {
        size += strlen(argv[i]) + 1;
    }

    char* prog_args = malloc(size);

    char* ptr = prog_args;

    for(int i = 0; argv[i] != NULL; i++) {
        strcpy(ptr, argv[i]);
        ptr += strlen(argv[i]) + 1;
    }



    struct sc_exec_args args = {
        .args = prog_args,
        .args_sz = 0,
        .new_process = 0,
    };

    syscall(SC_EXEC, &args, sizeof(args));


    free(prog_args);
}


int forkexec(const char* cmdline) {

    char* cmdline_cpy = malloc(strlen(cmdline) + 1);
    strcpy(cmdline_cpy, cmdline);

    struct sc_exec_args args = {
        .args = cmdline_cpy,
        .args_sz = strlen(cmdline)+1,
        .new_process = 1,
    };

    
    while(strrchr(cmdline_cpy, ' ') != NULL) {
        *(char *)strrchr(cmdline_cpy, ' ') = '\0';
    }

    
    syscall(SC_EXEC, &args, sizeof(args));
    free(cmdline_cpy);

    return 0;
}




int chdir (const char *__path) {
    struct sc_chdir_args args = {
        .path = __path,
        .path_len = strlen(__path),
    };

    return syscall(SC_CHDIR, &args, sizeof(args));
}

char *getcwd (char *buf, size_t size) {
    struct sc_getcwd_args args = {
        .buf = buf,
        .buf_sz = size,
    };

    return (char *)syscall(SC_GETCWD, &args, sizeof(args));
}


/* Duplicate FD, returning a new file descriptor on the same file.  */
int dup (int fd) {
    struct sc_dup_args args = {
        .fd  = fd,
        .fd2 = -1,
    };
    return syscall(SC_DUP, &args, sizeof(args));
}

/* Duplicate FD to FD2, closing FD2 and making it open on the same file.  */
int dup2 (int fd, int fd2) {
    struct sc_dup_args args = {
        .fd  = fd,
        .fd2 = fd2,
    };
    return syscall(SC_DUP, &fd, sizeof(fd));
}



/* Get the process ID of the calling process.  */
pid_t getpid (void) {
    return syscall(SC_GETPID, NULL, 0);
}

/* Get the process ID of the calling process's parent.  */
pid_t getppid (void) {
    return syscall(SC_GETPPID, NULL, 0);
}





int __attribute__ ((__const__)) getpagesize (void)  {
    return 0x1000;
}


void __attribute__ ((__noreturn__)) _exit (int status) {
    syscall(SC_EXIT, &status, sizeof(status));
    __builtin_unreachable();
}



int fopen(const char* path, const char* mode) {
    int flags = 0;
    if(strcmp(mode, "r") == 0) {
        flags = O_RDONLY;
    } else if(strcmp(mode, "w") == 0) {
        flags = O_WRONLY;
    } else if(strcmp(mode, "rw") == 0) {
        flags = O_RDWR;
    } else {
        return -1;
    }

    return open(path, flags, 0);
}
