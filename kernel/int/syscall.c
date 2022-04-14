#include "syscall.h"
#include "../lib/registers.h"
#include "../lib/dump.h"
#include "../lib/logging.h"
#include "../sched/sched.h"
#include "../memory/vmap.h"
#include "../memory/physical_allocator.h"
#include "../memory/paging.h"
#include "../memory/heap.h"
#include "../lib/time.h"
#include "../lib/string.h"
#include "../lib/stacktrace.h"
#include "apic.h"


#define IA32_EFER_SCE_BIT (1lu)


typedef uint64_t(*sc_fun_t)(process_t*, void*, size_t);

static sc_fun_t sc_funcs[SC_END];

static inline void sc_warn(const char* msg, void* args, size_t args_sz) {
/*
    log_debug("syscall process %u, thread %u: %s", 
             sched_current_pid(), 
             sched_current_tid(),
             msg,
             args_sz
    );   
*/
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
    for(unsigned i = 0 ;i < proc->program->n_segs; i++) {
        
        const struct elf_segment* seg = &proc->program->segs[i];

        if(arg_begin >= (uint64_t)seg->base 
        && arg_end   <  (uint64_t)seg->base + seg->length)
            // found
            return 0;
    }

    return 1;
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

    sleep(*(uint64_t*)args);


    return 0;
}



static uint64_t sc_sbrk(process_t* proc, void* args, size_t args_sz) {
    if(args_sz != 8) {
        sc_warn("bad args_sz", args, args_sz);
    }


    int64_t delta = *(int64_t*)args;
    // align delta on 
    log_warn("sbrk %ld", delta);

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
        log_warn("issou %u", exec_args->args_sz);
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

    file_handle_t* elf_file = vfs_open_file(path);

    free(path);
    
    if(!elf_file) {
        sc_warn("failed to open file", args, args_sz);
        return -1;
    }

    vfs_seek_file(elf_file, 0, SEEK_END);
    // load the program
    size_t file_sz = vfs_tell_file(elf_file);

    vfs_seek_file(elf_file, 0, SEEK_SET);

    void* elf_data = malloc(file_sz);

    size_t rd = vfs_read_file(elf_data, file_sz, 1, elf_file);

    assert(rd == file_sz);
    
    vfs_close_file(elf_file);


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


    if(!exec_args->new_process) {
        assert(0);
        // regular UNIX exec
        replace_process(proc, elf_data, file_sz);
        new_proc = proc;
    }
    else {
        unmap_user();

        // sched_create_process needs the user to be unmaped

        pid_t p = sched_create_process(proc->pid, elf_data, file_sz);

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
    
    if(set_process_entry_arguments(new_proc, 
        args_copy, cmd_args_sz, env_copy, env_sz
    )) {
        sc_warn("failed to set process arguments", args, args_sz);
        
        // even if the process creation failed, we 
        // still need to remap the process
        set_user_page_map(proc->page_dir_paddr);
        return -1;
    }

    // done :)
    // now we must restore the current process 
    // memory map before returning
    
    set_user_page_map(proc->page_dir_paddr);

    
    return 0;
}


static fd_t find_available_fd(process_t* proc) {
    for(fd_t i = 0; i < MAX_FDS; i++) {
        if(proc->fds[i].type == FD_NONE)
            return i;
    }
    log_warn("issou");

    return -1;
}

// insert the file and returrn file descriptor
// return -1 if no free fd
static fd_t insert_process_file(process_t* proc, file_handle_t* file) {
    for(unsigned i = 0; i < MAX_FDS; i++) {
        if(proc->fds[i].type == FD_NONE) {
            proc->fds[i].file = file;
            proc->fds[i].type = FD_FILE;
            return i;
        }
    }

    return -1;
}


// insert the dir and returrn file descriptor
// return -1 if no free fd
static fd_t insert_process_dir(process_t* proc, struct DIR* dir) {
    for(unsigned i = 0; i < MAX_FDS; i++) {
        if(proc->fds[i].type == FD_NONE) {
            proc->fds[i].dir = dir;
            proc->fds[i].type = FD_DIR;
            return i;
        }
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



    if(a->flags & O_DIR) {
        // opening as a directory
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
        file_handle_t* h = vfs_open_file(a->path);

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
            sc_warn("bad fd", args, args_sz);
            return -1;
    }
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
        sc_warn("bad fd", args, args_sz);
        return -1;
    }

    if(proc->fds[a->fd].type == FD_NONE) {
        sc_warn("bad fd", args, args_sz);
        return -1;
    }

    // check that the buffer is mapped
    check_args(proc, a->buf, a->count);

    switch(proc->fds[a->fd].type) {
        case FD_FILE:
            return vfs_read_file(a->buf, 1, a->count, proc->fds[a->fd].file);
            break;
        case FD_DIR:
            return read_dir(&proc->fds[a->fd], a->buf, a->count);
            break;
        default:
            sc_warn("bad fd", args, args_sz);
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
        sc_warn("bad fd", args, args_sz);
        return -1;
    }

    if(proc->fds[a->fd].type == FD_NONE) {
        sc_warn("bad fd", args, args_sz);
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

    struct DIR* dir = vfs_opendir(path);


    free(path);


    if(dir) {
        vfs_closedir(dir);  

        free(proc->cwd);
        proc->cwd = strdup(path);
    }
    else
        return -1;

    return 0;
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





// defined in syscall.s
extern void syscall_entry(void);



void syscall_init(void) {


    for(unsigned i = 0; i < SC_END; i++)
        sc_funcs[i] = sc_unimplemented;

    sc_funcs[SC_SLEEP]  = sc_sleep;
    sc_funcs[SC_SBRK]   = sc_sbrk;
    sc_funcs[SC_OPEN]   = sc_open;
    sc_funcs[SC_CLOSE]  = sc_close;
    sc_funcs[SC_SEEK]   = sc_seek;
    sc_funcs[SC_READ]   = sc_read;
    sc_funcs[SC_WRITE]  = sc_write;
    sc_funcs[SC_EXEC]   = sc_exec;
    sc_funcs[SC_CHDIR]  = sc_chdir;
    sc_funcs[SC_GETCWD] = sc_getcwd;
    sc_funcs[SC_CLOCK]  = sc_clock;
    sc_funcs[SC_DUP]    = sc_dup;
    sc_funcs[SC_GETPID] = sc_getpid;
    sc_funcs[SC_GETPPID]= sc_getppid;
    


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



// called from syscall_entry
uint64_t syscall_main(uint8_t scid, void* args, size_t args_sz) {

    //log_warn("SYSCALL %u", scid);

    process_t* process = sched_current_process();

    assert(process);

    //log_warn("SYSCALL %u, brk=%lx", scid, process->brk);

    if(scid >= SC_END) {
        log_warn("process %u, thread %u: bad syscall", sched_current_pid(), sched_current_tid());
        for(;;)
            asm ("hlt");
    }
    else
        return sc_funcs[scid](process, args, args_sz);
}
