#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

// for system call numbers
#include "../../kernel/int/syscall_interface.h"



static void* brk_addr = NULL;

/*
 * implement syscall wrappers 
 */


int usleep (uint64_t __useconds) {
    return syscall(SC_SLEEP, &__useconds, sizeof(__useconds));
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

    if(lseek(fd, offset, SEEK_SET) == (uint64_t)-1) {
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



/**
 * @brief create a string list:
 * strings separated by '\0'
 * with double '\0' at the end
 * 
 * @param len number of null-terminated strings in arr
 * @param arr null terminated array of null terminated
 *            strings
 * @param size (output) size of the resulting string 
 *            list buffer
 * @return mallocated string list
 */
static char* create_string_list(char const* const* arr, size_t* size) {

    // first compute the needed buffer size
    // and the number of strings
    size_t len = 0;
    size_t bufsize = 1; // for the last '\0'

    while(arr[len])
        bufsize += strlen(arr[len++]);


    bufsize += len; // for the '\0' between each string


    *size = bufsize;

    // allocate the buffer
    char* list = malloc(bufsize);

    

    // fill the buffer
    char* p = list;
    for(size_t i = 0; i < len; i++) {
        int slen = strlen(arr[i]);
        memcpy(p, arr[i], slen);
        p += slen;
        *p++ = '\0';
    }

    *p++ = '\0';
    return list;
}

// base function for all exec* functions
static
int exec(const char* file, 
         char const* const argv[], 
         char const* const envp[], 
         int new_process
) {
    // file is equal to argv[0]
    (void) file;

    size_t args_sz = 0, env_sz = 0;

    char* args = create_string_list(argv, &args_sz);
    char* env  = create_string_list(envp, &env_sz);


    struct sc_exec_args sc_args = {
        .args = args,
        .args_sz = args_sz,
        .env = env,
        .env_sz = env_sz,
        .new_process = new_process,
    };

    int r = syscall(SC_EXEC, &sc_args, sizeof(sc_args));

    free(args);
    free(env);

    return r;
}


int execvpe(const char* file, 
            char const* const argv[], 
            char const* const envp[]
) {
    if(strcmp(file, argv[0]) != 0) {
        return -1;
    };

    return exec(file, argv, envp, 0);
}


int execv (const char* path, char const* const argv[]) {
    // @todo search in $PATH
    return execvp(path, argv);
}

int execvp (const char *le, char const* const argv[]) {
    return exec(le, argv, (char const* const*)__environ, 0);
}


int forkexec(char const* const cmdline[]) {
    return exec(cmdline[0], cmdline, (char const* const*)__environ, 1);
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

    if(size == 0 &&  buf == NULL) {
        // reaquest cwd size first
        size = syscall(SC_GETCWD, &args, sizeof(args)); 

        if(size == (uint64_t)-1) {
            return NULL;
        }

        args.buf    = malloc(size);
        args.buf_sz = size;
    }



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
    return syscall(SC_DUP, &args, sizeof(args));
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
