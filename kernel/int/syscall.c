#include "syscall.h"
#include "../lib/registers.h"
#include "../lib/dump.h"
#include "../lib/logging.h"
#include "../sched/sched.h"
#include "../sched/thread.h"
#include "../memory/vmap.h"
#include "../memory/physical_allocator.h"
#include "../memory/paging.h"
#include "../memory/heap.h"
#include "../lib/time.h"
#include "../lib/string.h"
#include "../lib/panic.h"
#include "../lib/stacktrace.h"
#include "../int/idt.h"
#include "../fs/pipefs/pipefs.h"
#include "apic.h"


#define IA32_EFER_SCE_BIT (1lu)


typedef uint64_t(*sc_fun_t)(process_t*, void*, size_t);

static sc_fun_t sc_funcs[SC_END];

static inline void sc_warn(const char* msg, void* args, size_t args_sz) {
    (void) args;
    (void) args_sz;
    (void) msg;

    log_debug("syscall process %u, thread %u: %s", 
             sched_current_pid(), 
             sched_current_tid(),
             msg,
             args_sz
    );   
    stacktrace_print();

}


static int check_args_in_program(
                const process_t* proc, const void* args, size_t args_sz
) {

    const uint64_t arg_begin = (uint64_t)args;
    const uint64_t arg_end   = arg_begin + args_sz;

    
    // in the program
    for(unsigned i = 0 ;i < proc->program->n_segs; i++) {
        
        const struct elf_segment* seg = &proc->program->segs[i];

        if(arg_begin >= (uint64_t)seg->base 
        && arg_end   <  (uint64_t)seg->base + seg->length)
            // found
            return 0;
    }
    return 1;
}


// check that the adddress is in
// the process accessible range
// 0: fine
// non-0: check failed
static int check_args(const process_t* proc, const void* args, size_t args_sz) {
    // find a range in which the args are
    
    const uint64_t arg_begin = (uint64_t)args;
    const uint64_t arg_end   = arg_begin + args_sz;


    // in one program's thread stack
    for(unsigned i = 0; i < proc->n_threads; i++) {
        stack_t* tstack =&proc->threads[i].stack;
        if(arg_begin >= (uint64_t)tstack->base 
        && arg_end < (uint64_t)tstack->base + tstack->size)
            // found
            return 0;
    }


    // in the heap
    if(arg_begin >= (uint64_t)proc->heap_begin 
    && arg_end   <  (uint64_t)proc->brk)
        return 0;
        // found


    // in the program
    return check_args_in_program(proc, args, args_sz);
}


static char* get_absolute_path(const char* cwd, const char* path) {
    if(path[0] == '/') {
        // absolute path
        return strdup(path);
    }

    size_t cwd_sz = strlen(cwd);
    size_t path_sz = strlen(path);

    char* abs_path = malloc(cwd_sz + path_sz + 2);

    memcpy(abs_path, cwd, cwd_sz);
    abs_path[cwd_sz] = '/';
    memcpy(abs_path + cwd_sz + 1, path, path_sz + 1);

    return abs_path;
}




static uint64_t sc_unimplemented(process_t* proc, void* args, size_t args_sz) {
    (void) proc;
    sc_warn("unimplemented syscall", args, args_sz);
    return -1;
}



static uint64_t sc_sleep(process_t* proc, void* args, size_t args_sz) {
    if(args_sz != 8) {
        sc_warn("bad args_sz", args, args_sz);
    }

    (void) proc;

    uint64_t ms = *(uint64_t*)args;

    if(ms == 0)
        sched_yield();
    else 
        sleep(*(uint64_t*)args);


    return 0;
}



static uint64_t sc_sbrk(process_t* proc, void* args, size_t args_sz) {
    if(args_sz != 8) {
        sc_warn("bad args_sz", args, args_sz);
    }


    int64_t delta = *(int64_t*)args;
    // align delta on 

    void* old_brk = proc->brk;
    void* unaligned_new_brk = proc->unaligned_brk + delta;

    void* new_brk = (void*)(((uint64_t)unaligned_new_brk + 0xfff) & ~0xffflu);
    

    assert_aligned(old_brk, 0x1000);
    assert_aligned(new_brk, 0x1000);

    int64_t needed_pages = (int64_t)new_brk - (int64_t)old_brk;

    needed_pages >>= 12;

    // checks on new brk
    if(
        !is_user(new_brk)                    // avoid userspace overflow
     || unaligned_new_brk < proc->heap_begin // avoid underflow
     || (int64_t)available_pages()*0x1000 <= needed_pages     // avoid memory overflow
    ) {
        return -1;
    }


    if(needed_pages > 0) {
        // alloc pages
        alloc_pages(
            old_brk, 
            needed_pages, 
            PRESENT_ENTRY | PL_XD | PL_RW | PL_US 
            // userspace, writable, execute disable
        );
    }
    else {
        unmap_pages(
            (uint64_t)new_brk,
            -needed_pages
        );
    }


    proc->unaligned_brk = unaligned_new_brk;
    proc->brk = new_brk;


    return (uint64_t)old_brk;
}


static uint64_t sc_exit(process_t* proc, void* args, size_t args_sz) {
    if(args_sz != 8) {
        sc_warn("bad args_sz", args, args_sz);
    }

    (void) proc;

    uint64_t status = *(uint64_t*)args;

    sched_kill_process(sched_current_pid(), status);
    // every thread (including the current one)
    // is marked as exited.d
    // the process will is killed by the scheduler

    // atomically mark the current thread as ready
    /*
    _cli();
    spinlock_acquire(&proc->lock);
    proc->threads[sched_current_tid()].state = READY;
    spinlock_release(&proc->lock);


    _sti();
*/

    sched_yield();

    panic("reached unreachable code");
    __builtin_unreachable();
    //schedule();
}

/*
char* parse_cmdline(const char* cmdline, size_t cmdline_sz) {
    char* cmdline_copy = (char*)malloc(cmdline_sz);
    memcpy(cmdline_copy, cmdline, cmdline_sz);

    char* end = cmdline_copy + cmdline_sz;
    char* ptr = cmdline_copy;

    return cmdline_copy;
}
*/

static uint64_t sc_exec(process_t* proc, void* args, size_t args_sz) {
    if(args_sz != sizeof(struct sc_exec_args)) {
        sc_warn("bad args_sz", args, args_sz);
        return -1;
    }

    ////////////////////////////////////////////
    /////       checking arguments         /////
    ////////////////////////////////////////////

    struct sc_exec_args* exec_args = (struct sc_exec_args*)args;

    if(exec_args->args_sz < 1 || 
        check_args(proc, exec_args->args, exec_args->args_sz)) 
    {
        sc_warn("bad cmdline size", args, args_sz);
        return -1;
    }

    // this unsures that strlen won't generate a page fault
    if(exec_args->args[exec_args->args_sz - 1] != '\0'
    ) {
        sc_warn("bad cmdline", args, args_sz);
        return -1;
    }


    if(exec_args->env_sz < 1 || 
        check_args(proc, exec_args->env, exec_args->env_sz)) 
    {
        sc_warn("bad env vars", args, args_sz);
        return -1;
    }

    // this unsures that strlen won't generate a page fault
    if(exec_args->env[exec_args->env_sz - 1] != '\0'
    ) {
        sc_warn("bad environment vars", args, args_sz);
        return -1;
    }

    ////////////////////////////////////////////
    /////       open executable file       /////
    ////////////////////////////////////////////

    // first element of args is the program name
    char* path = get_absolute_path(proc->cwd, exec_args->args);

    file_handle_t* elf_file = vfs_open_file(path, VFS_READ);

    free(path);
    
    if(!elf_file) {
        sc_warn("failed to open elf file", args, args_sz);
        return -1;
    }

    vfs_seek_file(elf_file, 0, SEEK_END);
    // load the program
    size_t file_sz = vfs_tell_file(elf_file);

    vfs_seek_file(elf_file, 0, SEEK_SET);

    void* elf_data = malloc(file_sz);

    memset(elf_data, 0, file_sz);

    size_t rd = vfs_read_file(elf_data, file_sz, elf_file);

    vfs_close_file(elf_file);

    assert(rd == file_sz);

/*    
    // checksum
    int s = memsum(elf_data, file_sz);

    printf("checksum: %d\n", s);
*/

    // as we might to map another process
    // we won't be able to access args and env
    // so we copy them in kernel heap
    char* args_copy = (char*)malloc(exec_args->args_sz);
    char* env_copy  = (char*)malloc(exec_args->env_sz);

    memcpy(args_copy, exec_args->args, exec_args->args_sz);
    memcpy(env_copy,  exec_args->env,  exec_args->env_sz);

    // very bad naming....
    size_t cmd_args_sz = exec_args->args_sz;
    size_t env_sz = exec_args->env_sz;



    ////////////////////////////////////////////
    /////          create process          /////
    ////////////////////////////////////////////


    // resulting process
    // either the current process or a new one
    process_t* new_proc;


    // critical section: we can't be interrupted
    // the process is in the process table
    // but not fully initialized

    _cli();

    if(!exec_args->new_process) {
        assert(0);
        // regular UNIX exec
        replace_process(proc, elf_data, file_sz);
        new_proc = proc;
        free(elf_data);
    }
    else {
        unmap_user();

        // sched_create_process needs the user to be unmaped
        pid_t p = sched_create_process(
                        proc->pid, // PPID
                        elf_data, file_sz);
        free(elf_data);

        if(p == -1) {
            sc_warn("failed to create process", args, args_sz);

            // even if the process creation failed, we 
            // still need to remap the process
            set_user_page_map(proc->page_dir_paddr);

            return -1;
        }
    


        new_proc = sched_get_process(p);

    }

    // here, new_proc is currently mapped in memory
    assert(new_proc);


    ////////////////////////////////////////////
    //// set process arguments and env vars ////
    ////////////////////////////////////////////

    set_user_page_map(new_proc->page_dir_paddr);

    int res = set_process_entry_arguments(new_proc, 
        args_copy, cmd_args_sz, env_copy, env_sz
    );


    free(args_copy);
    free(env_copy);

    if(res) {
        sc_warn("failed to set process arguments", args, args_sz);

        // release process lock
        spinlock_release(&new_proc->lock);
        
        // even if the process creation failed, we 
        // still need to remap the process
        set_user_page_map(proc->page_dir_paddr);
        return -1;
    }

    pid_t new_pid = new_proc->pid;

    spinlock_release(&new_proc->lock);



    _sti();


    // unblock the process
    sched_unblock(new_pid, 1);
    

    // done :)
    // now we must restore the current process 
    // memory map before returning

    set_user_page_map(proc->page_dir_paddr);
/*
    dump(
        ((uint64_t)new_proc->threads[0].rsp & ~16llu),
        
        new_proc->threads[0].stack.base 
        + new_proc->threads[0].stack.size 
        - (void*)new_proc->threads[0].rsp,

        16,

        DUMP_HEX8
    );
*/

    //sched_yield();

    return 0;
}   

// return the fd if found or -1 otherwise
static fd_t find_available_fd(process_t* proc) {
    for(fd_t i = 0; i < MAX_FDS; i++) {
        if(proc->fds[i].type == FD_NONE)
            return i;
    }

    return -1;
}


static uint64_t sc_open(process_t* proc, void* args, size_t args_sz) {
    if(args_sz != sizeof(struct sc_open_args)) {
        sc_warn("bad args_sz", args, args_sz);
        return -1;
    }

    struct sc_open_args* a = args;

    check_args(proc, a->path, a->path_len);


    char* path = get_absolute_path(proc->cwd, a->path);


    if(strlen(path) > MAX_PATH) {
        sc_warn("path too long", args, args_sz);

        free(path);
        return -1;
    }


    fd_t fd = find_available_fd(proc);


    if(fd == (fd_t)-1) {
        sc_warn("no free fd", args, args_sz);

        free(path);
        return -1;
    }


    if(a->flags & O_DIRECTORY) {
        // opening as a directory

        // for now, we only support opening directories
        // with O_RDONLY
        if(a->flags & O_WRONLY) {
            sc_warn("writing to directory", args, args_sz);

            free(path);
            return -1;
        }
        else if(a->flags & O_TRUNC) {
            sc_warn("truncating directory", args, args_sz);

            free(path);
            return -1;
        }
        else if(a->flags & O_APPEND) {
            sc_warn("appending to directory", args, args_sz);

            free(path);
            return -1;
        }
        else if(a->flags & O_CREAT) {
            sc_warn("creating directory", args, args_sz);

            free(path);
            return -1;
        }

        struct DIR* dir = vfs_opendir(path);


        if(dir) {
            proc->fds[fd].dir = dir;
            proc->fds[fd].dir_boff = 0;
            proc->fds[fd].type = FD_DIR;
        }
        else {
            sc_warn("failed to open dir", args, args_sz);
            
            free(path);
            return -1;
        }
    }
    else {

        file_handle_t* h = vfs_open_file(path, a->flags);


        if(a->flags & O_TRUNC) {
            vfs_truncate_file(h, 0);
            h->file_offset = 0;
        }


        if(!h) {
            sc_warn("failed to open file", args, args_sz);

            free(path);
            return -1;
        }

        proc->fds[fd].file = h;
        proc->fds[fd].type = FD_FILE;
    }


    free(path);
    // insert h in the process file handles


    return fd;
}

uint64_t sc_truncate(process_t* proc, void* args, size_t args_sz) {
    if(args_sz != sizeof(struct sc_truncate_args)) {
        sc_warn("bad args_sz", args, args_sz);
        return -1;
    }

    struct sc_truncate_args* a = args;

    // check a->fd

    fd_t fd = a->fd;

    if(fd >= MAX_FDS) {
        sc_warn("bad fd", args, args_sz);
        return -1;
    }

    if(proc->fds[fd].type == FD_NONE) {
        sc_warn("fd not open", args, args_sz);
        return -1;
    }

    if(proc->fds[fd].type != FD_FILE) {
        sc_warn("can only truncate files", args, args_sz);
        return -1;
    }

    int res = vfs_truncate_file(proc->fds[fd].file, a->size);

    return res = 0 ? 0 : -1;
}



static uint64_t sc_close(process_t* proc, void* args, size_t args_sz) {
    if(args_sz != sizeof(fd_t)) {
        sc_warn("bad args_sz", args, args_sz);
        return -1;
    }

    fd_t fd = *(fd_t*)args;

    if(fd >= MAX_FDS) {
        sc_warn("bad fd", args, args_sz);
        return -1;
    }

    if(proc->fds[fd].file == NULL) {
        sc_warn("bad fd", args, args_sz);
        return -1;
    }


    return close_fd(&proc->fds[fd]);
}


static uint64_t seek_dir(file_descriptor_t* dird, int64_t offset, int whence) {
    assert(dird->type == FD_DIR);
    

    if(whence == SEEK_SET)
        dird->dir_boff = offset;
    else if(whence == SEEK_CUR) {
        dird->dir_boff += offset;
    }
    else if(whence == SEEK_END) {
        dird->dir_boff = offset + 
                    dird->dir->len * sizeof(struct dirent);
    }
    else {
        return -1;
    }
    return dird->dir_boff;
}


static uint64_t sc_seek(process_t* proc, void* args, size_t args_sz) {

    if(args_sz != sizeof(struct sc_seek_args)) {
        sc_warn("bad args_sz", args, args_sz);
        return -1;
    }


    struct sc_seek_args* a = args;

    if(a->fd >= MAX_FDS) {
        sc_warn("bad fd", args, args_sz);
        return -1;
    }

    if(proc->fds[a->fd].type == FD_NONE) {
        sc_warn("bad fd", args, args_sz);
        return -1;
    }

    switch(proc->fds[a->fd].type) {
        case FD_FILE:
            return vfs_seek_file(proc->fds[a->fd].file, a->offset, a->whence);
            break;
        case FD_DIR:
            return seek_dir(&proc->fds[a->fd], a->offset, a->whence);
            break;
        default:
            assert(0);
            return -1;
    }
}

#define	R_OK	4		/* Test for read permission.  */
#define	W_OK	2		/* Test for write permission.  */
#define	X_OK	1		/* Test for execute permission.  */
#define	F_OK	0		/* Test for existence.  */

static uint64_t sc_access(process_t* proc, void* args, size_t args_sz) {
    if(args_sz != sizeof(struct sc_access_args)) {
        sc_warn("bad args_sz", args, args_sz);
        return -1;
    }

    struct sc_access_args* a = args;

    check_args(proc, a->path, a->path_len);

    char* path = get_absolute_path(proc->cwd, a->path);

    if(strlen(path) > MAX_PATH) {
        sc_warn("path too long", args, args_sz);

        free(path);
        return -1;
    }
    
    fast_dirent_t d;
    fs_t* fs = vfs_open(path, &d);
    
    int ret = 0;

    if(fs == FS_NO)
        ret = -1;

    else if((a->type & (W_OK | R_OK | X_OK)) && (!fs || d.type == DT_DIR))
        ret = -1;

    else if(d.type == DT_DIR && (a->type & (R_OK|W_OK|X_OK)) != 0) 
        ret = -1;

    else if(a->type & W_OK && !d.rights.write)
        ret = -1;
    else if(a->type & R_OK && !d.rights.read)
        ret = -1;
    else if(a->type & X_OK && !d.rights.exec)
        ret = -1;

    free(path);

    return ret;
}


uint64_t read_dir(file_descriptor_t* dird, void* buf, size_t buf_sz) {
    assert(dird->type == FD_DIR);

    if(dird->dir_boff >= dird->dir->len * sizeof(struct dirent)) {
        return 0;
    }

    size_t bytes_to_read = dird->dir->len * sizeof(struct dirent) - dird->dir_boff;

    if(bytes_to_read > buf_sz) {
        bytes_to_read = buf_sz;
    }

    memcpy(buf, dird->dir->children + dird->dir_boff, bytes_to_read);


    dird->dir_boff += bytes_to_read;

    return bytes_to_read;
}


static uint64_t sc_read(process_t* proc, void* args, size_t args_sz) {
    if(args_sz != sizeof(struct sc_read_args)) {
        sc_warn("bad args_sz", args, args_sz);
        return -1;
    }

    struct sc_read_args* a = args;

    if(a->fd >= MAX_FDS) {
        sc_warn("read: bad fd", args, args_sz);
        return -1;
    }

    if(proc->fds[a->fd].type == FD_NONE) {
        sc_warn("read: bad fd", args, args_sz);
        return -1;
    }

    // check that the buffer is mapped
    check_args(proc, a->buf, a->count);

    switch(proc->fds[a->fd].type) {
        case FD_FILE:
        {
            size_t rd = vfs_read_file(a->buf, a->count, proc->fds[a->fd].file);

            return rd;
        }
            break;
        case FD_DIR:
            return read_dir(&proc->fds[a->fd], a->buf, a->count);
            break;
        default:
            assert(0);
            return -1;
    }
}


static uint64_t sc_write(process_t* proc, void* args, size_t args_sz) {
    if(args_sz != sizeof(struct sc_write_args)) {
        sc_warn("bad args_sz", args, args_sz);
        return -1;
    }

    struct sc_write_args* a = args;

    if(a->fd >= MAX_FDS) {
        sc_warn("write: bad fd", args, args_sz);
        return -1;
    }

    if(proc->fds[a->fd].type == FD_NONE) {
        sc_warn("write: bad fd", args, args_sz);
        return -1;
    }


    // check that the buffer is mapped
    check_args(proc, a->buf, a->count);


    switch(proc->fds[a->fd].type) {
        case FD_FILE:
            return vfs_write_file(a->buf, 1, a->count, proc->fds[a->fd].file);
            break;
        // we might use this for writing to a directory
        // to create new entries
        default:
            sc_warn("bad fd", args, args_sz);
            return -1;
    }
}



static uint64_t sc_chdir(process_t* proc, void* args, size_t args_sz) {
    if(args_sz != sizeof(struct sc_chdir_args)) {
        sc_warn("bad args_sz", args, args_sz);
        return -1;
    }

    struct sc_chdir_args* a = args;

    check_args(proc, a->path, a->path_len);


    if(a->path[a->path_len-1] != 0) {
        sc_warn("invalid path", args, args_sz);
        return -1;
    }

    if(a->path_len > MAX_PATH) {
        sc_warn("path too long", args, args_sz);
        return -1;
    }

    char* path = get_absolute_path(proc->cwd, a->path);



    if(strlen(path) > MAX_PATH) {
        sc_warn("path too long", args, args_sz);
        free(path);
        return -1;
    }


    fast_dirent_t d;
    fs_t* fs = vfs_open(path, &d);



    int res;


    if(fs != FS_NO && d.type == DT_DIR) {
        free(proc->cwd);
        proc->cwd = malloc(strlen(path) + 1);

        simplify_path(proc->cwd, path);

        res = 0;
    }
    else
        res = -1;

    free(path);

    return res;
}


static uint64_t sc_getcwd(process_t* proc, void* args, size_t args_sz) {
    if(args_sz != sizeof(struct sc_getcwd_args)) {
        sc_warn("bad args_sz", args, args_sz);
        return -1;
    }

    struct sc_getcwd_args* a = args;

    if(a->buf_sz == 0 && a->buf == NULL)
        return strlen(proc->cwd) + 1;

    check_args(proc, a->buf, a->buf_sz);

    if(a->buf_sz < strlen(proc->cwd) + 1) {
        sc_warn("path buf too short", args, args_sz);

        return (uint64_t)NULL;
    }


    strcpy(a->buf, proc->cwd);

    return (uint64_t)a->buf;
}


static uint64_t sc_getpid(process_t* proc, void* args, size_t args_sz) {
    if(args_sz != 0) {
        sc_warn("bad args_sz", args, args_sz);
        return -1;
    }

    return proc->pid;
}


static uint64_t sc_getppid(process_t* proc, void* args, size_t args_sz) {
    if(args_sz != 0) {
        sc_warn("bad args_sz", args, args_sz);
        return -1;
    }

    return proc->ppid;
}


static uint64_t sc_dup(process_t* proc, void* args, size_t args_sz) {
    if(args_sz != sizeof(struct sc_dup_args)) {
        sc_warn("bad args_sz", args, args_sz);
        return -1;
    }

    struct sc_dup_args* a = args;

    fd_t fd2 = a->fd2;

    if(a->fd >= MAX_FDS) {
        sc_warn("bad fd", args, args_sz);
        return -1;
    }


    if(proc->fds[a->fd].type == FD_NONE) {
        sc_warn("bad fd", args, args_sz);
        return -1;
    }

    if(fd2 != (fd_t)-1) {
        // dup2

        if(fd2 >= MAX_FDS) {
            sc_warn("bad fd2", args, args_sz);
            return -1;
        }

        if(proc->fds[fd2].type != FD_NONE) {
            sc_warn("fd2 already in use", args, args_sz);
            return -1;
        }
    }
    else {
        fd2 = find_available_fd(proc);

        if(fd2 == (fd_t)-1) {
            sc_warn("too many fds", args, args_sz);
            return -1;
        }
    }

    dup_fd(&proc->fds[a->fd], &proc->fds[fd2]);

    return fd2;
}


static uint64_t sc_clock(process_t* proc, void* args, size_t args_sz) {
    if(args_sz != 0) {
        sc_warn("bad args_sz", args, args_sz);
        return -1;
    }

    return clock_ns() - proc->clock_begin;
}



static uint64_t sc_pipe(process_t* proc, void* args, size_t args_sz) {
    if(args_sz != 0) {
        sc_warn("bad args_sz", args, args_sz);
        return -1;
    }

    file_handle_pair_t pair;

    int res = create_pipe(&pair);

    if(res)
        return -1;
    // if no error occurs, let's assign the fds


    fd_t fd1 = find_available_fd(proc);

    if(fd1 == (fd_t)-1) {
        sc_warn("too many fds", args, args_sz);
        
        vfs_close_file(pair.in);
        vfs_close_file(pair.out);

        return -1;
    }

    proc->fds[fd1].type = FD_FILE;
    proc->fds[fd1].file = pair.out;


    fd_t fd2 = find_available_fd(proc);

    if(fd2 == (fd_t)-1) {
        sc_warn("too many fds", args, args_sz);

        vfs_close_file(pair.in);
        vfs_close_file(pair.out);

        return -1;
    }


    proc->fds[fd2].type = FD_FILE;
    proc->fds[fd2].file = pair.in;

    return (uint64_t)fd2 << 32 | fd1;
}


////////////////////////////////////////
//////// SIGNALSYSCALL HANDLERS ////////
////////////////////////////////////////

uint64_t sc_sigsetup(process_t* proc, void* args, size_t args_sz) {
    if(args_sz != sizeof(struct sc_sigsetup_args)) {
        sc_warn("bad args_sz", args, args_sz);
        return -1;
    }

    struct sc_sigsetup_args* a = args;

    // check that handler_table is mapped
    check_args(proc, args, args_sz);
    check_args_in_program(proc, a->handler_table, sizeof(void*) * MAX_SIGNALS);
    
    uint64_t res;

    _cli();
    spinlock_acquire(&proc->lock);
    {
        res = process_register_signal_setup(proc, a->signal_end, a->handler_table);
    }
    

    spinlock_release(&proc->lock);
    _sti();

    return res;
}



uint64_t sc_sigretrn(process_t* proc, void* args, size_t args_sz) {
    if(args_sz != 0) {
        sc_warn("bad args_sz", args, args_sz);
        return -1;
    }

    uint64_t res;

    _cli();
    spinlock_acquire(&proc->lock);
    {
        res = process_end_of_signal(proc);
    }
    spinlock_release(&proc->lock);
    _sti();

    return res;
}


uint64_t sc_kill(process_t* proc, void* args, size_t args_sz) {

    (void) proc;


    if(args_sz != sizeof(struct sc_sigkill_args)) {
        sc_warn("bad args_sz", args, args_sz);
        return -1;
    }

    struct sc_sigkill_args* a = args;
    pid_t pid = a->pid;
    int signal = a->signal;


    if(signal < 0 || signal >= MAX_SIGNALS) {
        sc_warn("bad signal", args, args_sz);
        return -1;
    }

    if(pid == 0) {
        // @todo maybe signal to kernel thraed to 
        // shutdown / reboot / ...
        sc_warn("bad pid", args, args_sz);
        return -1;
    }


    return process_trigger_signal(pid, signal);
}


uint64_t sc_pause(process_t* proc, void* args, size_t args_sz) {

    (void) proc;
    (void) args;

    if(args_sz != 0) {
        sc_warn("bad args_sz", args, args_sz);
        return -1;
    }

    thread_pause();

    return 0;
}




// defined in syscall.s
extern void syscall_entry(void);



void syscall_init(void) {


    for(unsigned i = 0; i < SC_END; i++)
        sc_funcs[i] = sc_unimplemented;

    sc_funcs[SC_SLEEP]     = sc_sleep;
    sc_funcs[SC_SBRK]      = sc_sbrk;
    sc_funcs[SC_EXIT]      = sc_exit;
    sc_funcs[SC_OPEN]      = sc_open;
    sc_funcs[SC_CLOSE]     = sc_close;
    sc_funcs[SC_SEEK]      = sc_seek;
    sc_funcs[SC_ACCESS]    = sc_access;
    sc_funcs[SC_READ]      = sc_read;
    sc_funcs[SC_WRITE]     = sc_write;
    sc_funcs[SC_EXEC]      = sc_exec;
    sc_funcs[SC_CHDIR]     = sc_chdir;
    sc_funcs[SC_GETCWD]    = sc_getcwd;
    sc_funcs[SC_CLOCK]     = sc_clock;
    sc_funcs[SC_DUP]       = sc_dup;
    sc_funcs[SC_PIPE]      = sc_pipe;
    sc_funcs[SC_GETPID]    = sc_getpid;
    sc_funcs[SC_GETPPID]   = sc_getppid;
    sc_funcs[SC_SIGSETUP]  = sc_sigsetup;
    sc_funcs[SC_SIGRETURN] = sc_sigretrn;
    sc_funcs[SC_SIGKILL]   = sc_kill;
    sc_funcs[SC_SIGPAUSE]  = sc_pause;


    /*
        this bit enables the 'system call extentions'
        it allows us to use use the syscall instruction 
    */
    write_msr(IA32_EFER_MSR, read_msr(IA32_EFER_MSR) | IA32_EFER_SCE_BIT);


    // EFLAGS complement mask (unused for now): 0 mask

    // we need to disable interrupts right when entering system call
    // because syscall doesn't switch to the kernel stack
    write_msr(IA32_FMASK_MSR, 
                (read_msr(IA32_FMASK_MSR) & ~0xffffffffllu) // reserved part
                | (1 << 9) // Interrupt flag
    );

    // syscall RIP target

    write_msr(IA32_LSTAR_MSR, (uint64_t)syscall_entry);

    /**
     * target SYSCALL CS: IA32_STAR[47:32]
     * target SYSCALL SS: IA32_STAR[47:32] + 8
     * 
     * target SYSRET CS: IA32_STAR[63]:48] + 16
     * target SYSRET SS: IA32_STAR[63]:48] + 8
     * 
     */
    write_msr(IA32_STAR_MSR, (read_msr((IA32_EFER_MSR) & 0xffffffff)) 
                    | (((uint64_t)((USER_DS - 8) << 16) | KERNEL_CS) << 32lu));
}



char* scname[] = {
    "NULL",
    "SLEEP",            
    "CLOCK", 
    "EXIT", 
    "OPEN", 
    "CLOSE", 
    "READ", 
    "WRITE",
    "TRUNCATE", 
    "SEEK", 
    "ACCESS",
    "DUP", 
    "CREATE_THREAD", 
    "JOIN_THREAD", 
    "EXIT_THREAD", 
    "SBRK", 
    "FORK", 
    "EXEC", 
    "CHDIR", 
    "GETCWD", 
    "GETPID", 
    "GETPPID",
    "SC_SIGSETUP", 
    "SC_SIGRETURN",
    "SC_SIGKILL",
    "SC_SIGPAUSE",
};


// called from syscall_entry
uint64_t syscall_main(uint8_t scid, void* args, size_t args_sz, uint64_t* user_sp) {


    process_t* process = sched_current_process();


    // save the user stack pointer of the thread
    // while the irqs are still disable
    thread_t* t = sched_get_thread_by_tid(process, sched_current_tid());

    t->syscall_user_rsp = user_sp;


    assert(process->pid > 0 && process->pid <= MAX_PID);

    assert(process);

    // Though we might take the lock again,
    // we don't need  to hold it here
    // because it the process structure
    // cannot change while we are in the syscall
    // as we are in the kernel stack 
    spinlock_release(&process->lock);

    // this sti enables interrupts after releasing the lock
    // but the interrupts were disabled before and need
    // to be enabled again
    _sti();


    if(!scid && scid >= SC_END) {
        log_info("process %u, thread %u: bad syscall", sched_current_pid(), sched_current_tid());
        for(;;)
            asm ("hlt");
    }
    else {
        // log_debug("%u.%u: %s", sched_current_pid(), sched_current_tid(), scname[scid]);
        uint64_t res = sc_funcs[scid](process, args, args_sz);


        // disable interrupts again
        // to swich to the user stack
        _cli();
        
        spinlock_acquire(&process->lock);
        t = sched_get_thread_by_tid(process, sched_current_tid());
        
        t->syscall_user_rsp = NULL;

        spinlock_release(&process->lock);

        return res;
    }
}
