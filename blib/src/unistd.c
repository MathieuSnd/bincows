#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sprintf.h>

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

    // replace argv[0] by file
    int size = 0;
    
    for(unsigned i = 0; argv[i]; i++)
        size ++;
    

    const char** _argv = malloc(sizeof(char*) * (size + 1));

    memcpy(_argv + 1, argv + 1, sizeof(char*) * size);

    _argv[0] = file;

    size_t args_sz = 0, env_sz = 0;

    char* args = create_string_list(_argv, &args_sz);
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

    free(_argv);

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


static int try_access(const char* file, int mode) {
    return access(file, mode);
}


static char* search_path(const char* file) {
    if(file[0] == '/' || try_access(file, O_RDONLY) == 0) {
        return strdup(file);
    }
    
    // search for file in path
    // path is a null terminated string
    // path_sz is the size of the path buffer
    // file is a null terminated string

    char* buf = malloc(PATH_MAX);

    char* path = getenv("PATH");

    char* p = strtok(path, ":");

    while(p) {
        if(strlen(p) + strlen(file) + 2 > PATH_MAX)
            continue;

        sprintf(buf, "%s/%s", p, file);

        if(try_access(buf, O_RDONLY) == 0) {
            // found
            return buf;
        }

        p = strtok(NULL, ":");
    }

    free(buf);
    return NULL;
}



int execv (const char* path, char const* const argv[]) {
    char* path_buf = search_path(path);
    
    if(!path_buf) {
        return -1;
    }
    
    int res = exec(path_buf, argv, 
                (char const* const*)__environ, 0);

    free(path_buf);

    return res;
}


int execvp (const char *le, char const* const argv[]) {
    return exec(le, argv, (char const* const*)__environ, 0);
}


int forkexec(char const* const argv[]) {
    char* path_buf = search_path(argv[0]);
    
    if(!path_buf) {
        return -1;
    }
    
    int res = exec(path_buf, argv, (char const* const*)__environ, 1);


    free(path_buf);

    return res;
}




int chdir (const char *__path) {
    struct sc_chdir_args args = {
        .path = __path,
        .path_len = strlen(__path) + 1,
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

int access (const char* name, int type) {
    struct sc_access_args args = {
        .path = name,
        .path_len = strlen(name) + 1,
        .type = type,
    };

    return syscall(SC_ACCESS, &args, sizeof(args));
}


/* Truncate FILE to LENGTH bytes.  */
int truncate (const char* file, off_t length) {
    int fd = open(file, O_RDWR, 0);

    if(!fd)
        return -1;

    int res = ftruncate(fd, length);

    close(fd);

    return res;
}

/* Truncate the file FD is open on to LENGTH bytes.  */
int ftruncate (int fd, off_t length) {
    struct sc_truncate_args args = {
        .fd = fd,
        .size = length,
    };
    

    return syscall(SC_TRUNCATE, &args, sizeof(args));
}


/* Make all changes done to FD actually appear on disk.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
int fsync (int __fd) {
    (void) __fd;
    sync();

    return 0;
}

/* Make all changes done to all files actually appear on disk.  */
void sync (void) {
    usleep(0);
}


unsigned int sleep(unsigned int seconds) {
    syscall(SC_SLEEP, &seconds, sizeof(seconds));
    return 0;
}


int __attribute__ ((__const__)) getpagesize (void)  {
    return 0x1000;
}


void __attribute__ ((__noreturn__)) _exit (int status) {
    uint64_t sc_args = status;
    syscall(SC_EXIT, &status, sizeof(sc_args));
    __builtin_unreachable();
}


int pause (void) {
    syscall(SC_SIGPAUSE, NULL, 0);
    return 0;
}


int pipe (int pipedes[2]) {
    uint64_t res = syscall(SC_PIPE, NULL, 0);

    if(res == (uint64_t)-1)
        return -1;

    pipedes[0] = res & 0xffffffff;
    pipedes[1] = res >> 32;
}

