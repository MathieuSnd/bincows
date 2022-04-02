#include "syscall.h"
#include "../lib/registers.h"
#include "../lib/dump.h"
#include "../lib/logging.h"
#include "../sched/sched.h"
#include "../memory/vmap.h"
#include "../memory/physical_allocator.h"
#include "../memory/paging.h"

#define IA32_EFER_SCE_BIT (1lu)


typedef uint64_t(*sc_fun_t)(process_t*, void*, size_t);

static sc_fun_t sc_funcs[SC_END];

static inline void sc_warn(const char* msg, void* args, size_t args_sz) {
    log_warn("syscall process %u, thread %u: %s,    \targs_sz=%u, args:", 
             sched_current_pid(), 
             sched_current_tid(),
             msg,
             args_sz
    );   

    dump(
        args, 
        args_sz,
        16,
        DUMP_HEX8
    );
}


// check that the adddress is in
// the process accessible range
// 0: fine
// non-0: check failed
static int check_args(const process_t* proc, void* args, size_t args_sz) {
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

        if(arg_begin >= seg->base 
        && arg_end < seg->base + seg->length)
            // found
            return 0;
    }

    return 1;
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

    sc_warn("sleep", args, args_sz);
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

    
    // checks on new brk
    if(
        !is_user(new_brk)                    // avoid userspace overflow
     || unaligned_new_brk < proc->heap_begin // avoid underflow
     || available_pages()*0x1000 > delta     // avoid memory overflow
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
            new_brk,
            -needed_pages
        );
    }


    proc->unaligned_brk = unaligned_new_brk;
    proc->brk = new_brk;


    return old_brk;
}


// insert the file and returrn file descriptor
static fd_t insert_process_file(process_t* proc, file_handle_t* file) {
    for(unsigned i = 0; i < MAX_FDS; i++) {
        if(proc->files[i] == NULL) {
            proc->files[i] = file;
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

    if(a->path_len > MAX_PATH) {
        sc_warn("path too long", args, args_sz);
        return -1;
    }


    file_handle_t* h = vfs_open_file(a->path);

    if(!h) {
        return -1;
    }


    // insert h in the process file handles


    fd_t fd = insert_process_file(proc, h);

    if(fd == -1) {
        sc_warn("too many open files", args, args_sz);

        vfs_close_file(h);
        return -1;
    }

    return fd;
}


static uint64_t sc_close(process_t* proc, void* args, size_t args_sz) {
    if(args_sz != 8) {
        sc_warn("bad args_sz", args, args_sz);
        return -1;
    }

    fd_t fd = *(fd_t*)args;

    if(fd >= MAX_FDS) {
        sc_warn("bad fd", args, args_sz);
        return -1;
    }

    if(proc->files[fd] == NULL) {
        sc_warn("bad fd", args, args_sz);
        return -1;
    }

    vfs_close_file(proc->files[fd]);
    proc->files[fd] = NULL;

    return 0;
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

    if(proc->files[a->fd] == NULL) {
        sc_warn("bad fd", args, args_sz);
        return -1;
    }

    return vfs_seek_file(proc->files[a->fd], a->offset, a->whence);
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

    if(proc->files[a->fd] == NULL) {
        sc_warn("bad fd", args, args_sz);
        return -1;
    }

    return vfs_read_file(a->buf, a->count, 1, proc->files[a->fd]);
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

    if(proc->files[a->fd] == NULL) {
        sc_warn("bad fd", args, args_sz);
        return -1;
    }

    return vfs_write_file(a->buf, a->count, 1, proc->files[a->fd]);
}









// defined in syscall.s
extern void syscall_entry(void);



void syscall_init(void) {


    for(unsigned i = 0; i < SC_END; i++)
        sc_funcs[i] = sc_unimplemented;

    sc_funcs[SC_SLEEP] = sc_sleep;
    sc_funcs[SC_SBRK]  = sc_sbrk;
    sc_funcs[SC_OPEN]  = sc_open;
    sc_funcs[SC_CLOSE] = sc_close;
    sc_funcs[SC_SEEK]  = sc_seek;
    sc_funcs[SC_READ]  = sc_read;
    sc_funcs[SC_WRITE] = sc_write;
    


    /*
        this bit enables the 'system call extentions'
        it allows us to use use the syscall instruction 
    */
    write_msr(IA32_EFER_MSR, read_msr(IA32_EFER_MSR) | IA32_EFER_SCE_BIT);


    // EFLAGS complement mask (unused for now): 0 mask

    write_msr(IA32_FMASK_MSR, read_msr(IA32_FMASK_MSR) & ~0xffffffff);

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
    write_msr(IA32_STAR_MSR, (read_msr((IA32_EFER_MSR) & 0xffffffff) 
                    | ((uint64_t)((USER_DS - 8) << 16) | KERNEL_CS) << 32lu));
}



// called from syscall_entry
uint64_t syscall_main(uint8_t scid, void* args, size_t args_sz) {

    log_warn("SYSCALL %u", scid);

    process_t* process = sched_current_process();

    assert(process);

    if(scid >= SC_END) {
        log_warn("process %u, thread %u: bad syscall", sched_current_pid(), sched_current_tid());
        for(;;)
            asm ("hlt");
    }
    else
        return sc_funcs[scid](process, args, args_sz);
}
