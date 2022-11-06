#include "../lib/assert.h"
#include "../lib/time.h"
#include "../memory/heap.h"
#include "../memory/vmap.h"
#include "../memory/paging.h"
#include "../lib/logging.h"
#include "../lib/string.h"
#include "../int/apic.h"
#include "../int/idt.h"

#include "sched.h"

#define FIRST_TID 1
#define STACK_SIZE (32 * 1024)



// return stack base
// we suppose that the process is already mapped
// we find a stack address by polling user top memory 
static void* alloc_stack(process_t* process) {
    assert(!interrupt_enable());

    void* base = (void*)(USER_END + 1) - STACK_SIZE;


    assert(process);


    // search for unused memory
    for(unsigned i = 0; i < process->n_threads; i++) {
        // assume every thread is the same stack size
        if(base == process->threads[i].stack.base) {
            // @todo maybe put some padding between
            // thread stacks    
            base -= STACK_SIZE;
            
            // reloop
            i = -1;
        }
    }


//    log_info("%u/%u stack base: %lx", process->pid, process->n_threads, base);

    alloc_pages(
        base, 
        STACK_SIZE >> 12, 
        PRESENT_ENTRY | PL_US | PL_RW
    );

    return base;
}

static void free_stack(void* addr) {
    // assume every thread is the same stack size
    unmap_pages(
        (uint64_t)addr, 
        STACK_SIZE >> 12,
        1
    );  
}


void dup_fd(file_descriptor_t* fd, file_descriptor_t* new_fd) {
    new_fd->type = fd->type;

    switch(fd->type) {
        case FD_FILE:
            new_fd->file = vfs_handle_dup(fd->file);
            break;
        case FD_DIR:
            new_fd->dir = vfs_dir_dup(fd->dir);
            new_fd->dir_boff = fd->dir_boff;
            break;
        case FD_NONE:
            break;
        default:
            assert(0);
    }
}


int close_fd(file_descriptor_t* fd) {
    assert(interrupt_enable());
    if(fd->file == NULL) {
        return -1;
    }


    switch(fd->type) {
        case FD_FILE:
            vfs_close_file(fd->file);
            break;
        case FD_DIR:
            vfs_closedir(fd->dir);
            break;
        default:
            assert(0);
    }

    fd->type = FD_NONE;

    return 0;
}



static int create_main_thread(process_t* process) {
    assert(process->n_threads == 0);
    assert(!interrupt_enable());


    void* stack_base = alloc_stack(process);

    if(!stack_base) {
        log_warn("could not allocate stack for thread %u:%u", process->pid, 1);
        return -1;
    }
    

    int res = create_thread(
        &process->threads[0], 
        process->pid, 
        stack_base,
        STACK_SIZE, 
        FIRST_TID,
        (uint64_t)process->program->entry, // PC
        0 // rdi register
    );

    if(res) {
        free_stack(stack_base);
        log_warn("cannot create thread %u:%u", process->pid, 1);
        return -1;
    }

    process->n_threads = 1;

    return 0;
}

int create_process(
        process_t*  process, 
        process_t*  pparent, 
        const void* elffile, 
        size_t      elffile_sz,
        fd_mask_t  fd_mask
) {
    assert(process);
    assert(!interrupt_enable());

    // assert that no userspace is mapped
    assert(get_user_page_map() == 0);

    uint64_t user_page_map = alloc_user_page_map();

    set_user_page_map(user_page_map);

    // we have to have the userspace map ready
    // before calling this
    elf_program_t* program = elf_load(elffile, elffile_sz);

    
    if(!program) {
        log_debug("create_process: elf_load failed");
        return 0;
    }

    pid_t ppid = KERNEL_PID;

    file_descriptor_t* fds = NULL;

    fds = malloc(sizeof(file_descriptor_t) * MAX_FDS);

    char* cwd;

    // inherit parent file handlers
    ppid = pparent->pid;

    for(unsigned i = 0; i < MAX_FDS; i++) {
        if((fd_mask & 1) == 0) {
            dup_fd(pparent->fds + i, fds + i);
        }
        else {
            fds[i].type = FD_NONE;
        }
        fd_mask >>= 1;
    }


    // inherit parent directory
    cwd = strdup(pparent->cwd);



    thread_t* threads = malloc(sizeof(thread_t));


    pid_t pid = alloc_pid();


    // choose an emplacement for heap base
    // choose it above the highest elf 
    // segment's end
    void* elf_end = NULL;
    for(unsigned i = 0; i < program->n_segs; i++) {
        void* segment_end = program->segs[i].base + program->segs[i].length;
     
        if((uint64_t)elf_end < (uint64_t)segment_end)
            elf_end = segment_end;
    }

    *process = (process_t) {
        .fds   = fds,
        .n_threads = 0, // no thread yet. The thread is added right after
        .threads = threads,
        .page_dir_paddr = user_page_map,
        .saved_page_dir_paddr = user_page_map,
        .pid = pid,
        .ppid = ppid,
        .cwd  = cwd,
        .program = program,
        .clock_begin = clock_ns(),


        .sig_table = NULL,
        .sig_current = NOSIG,
        .sig_pending = 0,
        .sig_return_context = NULL,
        .sig_return_function = NULL,
    };

    // empty heap
    process->brk = process->unaligned_brk = process->heap_begin = 
                        (void *)(((uint64_t)elf_end+0xfff) & ~0x0fffllu);



    
    if(create_main_thread(process)) {
        free(cwd);
        free(threads);
        free(fds);
        free_user_page_map(user_page_map);
        elf_free(program);
        return 0;
    }


    return 1;
}


extern void kernel_process_entry(void);

int create_kernel_process(process_t* process) {

    // create a process with a single thread
    thread_t* threads = malloc(sizeof(thread_t));
    *threads = (thread_t) {
        .exit_hooks = NULL,
        .n_exit_hooks = 0,
        .lock = 0,
        .pid = 0,
        .tid = 1,
        .state = BLOCKED,
        .running_cpu_id = 0,
        .should_exit = 0,
        .uninterruptible = 0,
        .stack = (stack_t) {
            .base = NULL,
            .size = 0,
        },
        .kernel_stack = (stack_t) {
        .base = malloc(KERNEL_THREAD_STACK_SIZE),
        .size = KERNEL_THREAD_STACK_SIZE,
        },
    };


    void* stack_top = threads->kernel_stack.base 
                      + threads->kernel_stack.size;

    uint64_t* saved_frame = (uint64_t*)stack_top - 1;

    // frame top
    *saved_frame = (uint64_t)0;


    threads->rsp = stack_top - sizeof(gp_regs_t) - 8;

    assert(process);

    // set initial PC
    threads[0].rsp->rip = (uint64_t)kernel_process_entry;

    threads->rsp->cs  = KERNEL_CS;
    threads->rsp->ss  = KERNEL_DS;
    threads->rsp->rsp = (uint64_t)threads->rsp;

    threads->rsp->rflags = USER_RF; // doesn't really matter

    *process = (process_t) {
        .n_threads = 1,
        .threads = threads,
        .page_dir_paddr = 0,
        .saved_page_dir_paddr = 0,
        .pid  = 0,
        .ppid = 0,
        .cwd  = strdup("/"), // root directory
        .program = NULL,
        .clock_begin = clock_ns(),

        .sig_table = NULL,
        .sig_current = NOSIG,
        .sig_pending = 0,
        .sig_return_context = NULL,
        .sig_return_function = NULL,
    };




    process->fds = malloc(sizeof(file_descriptor_t) * MAX_FDS);

    for(unsigned i = 0; i < MAX_FDS; i++) {
        process->fds[i].type = FD_NONE;
        process->fds[i].file = NULL;
    }



    log_info("kernel process created");

    return 1;
}



void free_process(process_t* process) {

    assert(interrupt_enable());
    assert(process);
    assert(!process->n_threads);

    if(process->pid == KERNEL_PID) {
        log_info("free kernel process");
        // kernel process
    }

    for(unsigned i = 0; i < MAX_FDS; i++) {
        if(process->fds[i].type != FD_NONE) {
            close_fd(&process->fds[i]);
        }
    }

    free(process->fds);
    free(process->cwd);


    if(process->threads)
        // thread buffer hasn't been freed yet
        free(process->threads);

    if(process->program)
        elf_free(process->program);


    free(process);
}



int replace_process(process_t* process, void* elffile, size_t elffile_sz) {
    // assert that the process is already mapped
    assert(get_user_page_map() == process->page_dir_paddr);

    unmap_user();
    free_user_page_map(process->page_dir_paddr);

    // recreate page directory
    process->saved_page_dir_paddr =
    process->page_dir_paddr = alloc_user_page_map();

    set_user_page_map(process->page_dir_paddr);


    // only one thread
    process->threads = realloc(process->threads, sizeof(thread_t));


    // load program
    elf_program_t* program = elf_load(elffile, elffile_sz);


    // choose an emplacement for heap base
    // choose it above the highest elf 
    // segment's end
    void* elf_end = NULL;
    for(unsigned i = 0; i < program->n_segs; i++) {
        void* segment_end = program->segs[i].base + program->segs[i].length;
     
        if((uint64_t)elf_end < (uint64_t)segment_end)
            elf_end = segment_end;
    }

    // empty heap
    process->brk = process->unaligned_brk = process->heap_begin = 
                        (void *)(((uint64_t)elf_end+0xfff) & ~0x0fffllu);


    // this field is needed by alloc_stack
    process->n_threads = 0;



    if(create_main_thread(process)) {
        // couldn't create main thread
        free_process(process);
    }
    
    return 0;
}



/**
 *  
 * return an unsued, non-0 tid on success
 * bellow MAX_TID
 * 
 * 0 on success
 * 
 * 
 */
static tid_t alloc_tid(process_t* p) {
    assert(!interrupt_enable());

    tid_t pref = 1, alt = 1;

    // pref: prefered tid = max tid + 1
    // alt:  alterative tid, tid different to any other


    for(unsigned i = 0; i < p->n_threads; i++) {
        thread_t* t = p->threads + i;

        if(t->tid == alt)
            alt++;

        if(t->tid >= pref)
            pref = t->tid + 1;
    }

    if(pref <= MAX_TID) 
        return pref;
    else if(alt <= MAX_TID)
        return alt;

    else {// too much threads, no available tid
        log_warn("no available tid for process %u", p->pid);
        return 0;
    }
}


tid_t process_create_thread(process_t* proc, void* entry, void* argument) {
    assert(!interrupt_enable());

    // assert that the process is already mapped
    assert(get_user_page_map() == proc->page_dir_paddr);

    if(!proc)
        return 0;

    tid_t tid = alloc_tid(proc);

    if(tid) {

        void* stack_base = alloc_stack(proc);

        if(!stack_base) {
            log_warn("couldn't allocate stack for thread %u:%u", proc->pid, tid);
            return 0;
        }


        proc->threads = realloc(proc->threads, sizeof(thread_t) * (proc->n_threads + 1));
            

        thread_t* t = proc->threads + proc->n_threads;
        
        int res = create_thread(
            t,
            proc->pid,
            stack_base, 
            STACK_SIZE, 
            tid,
            (uint64_t) entry,
            (uint64_t) argument
        );

        if(!res) {
            (*((volatile size_t*)&proc->n_threads))++;

            sched_launch_thread(t);
        }
        else {
            
            log_warn("couldn't create thread %u:%u", proc->pid, tid);
        }
    }
    // else, no available tid
    

    return tid;
}





static size_t string_list_len(char* list) {
    size_t len = 0;

    while(*list) {
        list += strlen(list) + 1;
        len++;
    }

    return len;
}


int set_process_entry_arguments(process_t* process, 
    char* argv, size_t argv_sz, char* envp, size_t envp_sz
) {

    assert(process);
    //setup stack

    uint64_t rsp = (uint64_t)process->threads[0].stack.base
                 + process->threads[0].stack.size;


    // align sizes to 16 bytes
    // because we are copying it in
    // a stack
    argv_sz = (argv_sz + 15) & ~0x0f;
    envp_sz = (envp_sz + 15) & ~0x0f;

    // put the arguments and env vars in the stack
    uint64_t user_argv = (rsp -= argv_sz);
    uint64_t user_envp = (rsp -= envp_sz);

    // frame begin
    uint64_t* user_frame_begin = (uint64_t*)(rsp -= sizeof(uint64_t));

    *user_frame_begin = 0;


    process->threads[0].rsp = (void*)(rsp -= sizeof(gp_regs_t));



    // stack.base < rsp < user_envp < user_argv
    if((void*)process->threads[0].rsp <= process->threads[0].stack.base) {
        // not enough space
        return -1;
    }

    // copy argv and envp
    memcpy((void*)user_argv, argv, argv_sz);
    memcpy((void*)user_envp, envp, envp_sz);

    // compute argc and envc
    int argc;
    int envc;

    if(argv)
        argc = string_list_len(argv);
    else 
        argc = 0;

    if(envp)
        envc = string_list_len(envp);
    else 
        envc = 0;

    // fill registers initial values

    // entry arguments:
    // _start(int argc, char* args, int envrionc, char* environ)
    process->threads[0].rsp->rdi = argc;
    process->threads[0].rsp->rsi = user_argv;
    process->threads[0].rsp->rdx = envc;
    process->threads[0].rsp->rcx = user_envp;


    process->threads[0].rsp->rsp = rsp;
    process->threads[0].rsp->rbp = (uint64_t)user_frame_begin;
    process->threads[0].rsp->rip = (uint64_t)process->program->entry;
    process->threads[0].rsp->cs  = USER_CS;
    process->threads[0].rsp->ss  = USER_DS;
    process->threads[0].rsp->rflags = USER_RF;


    for(int i = 0; i < 8; i++)
        process->threads[0].rsp->ext[i] = 0;
    
    // zero the other registers
    process->threads[0].rsp->rax = 0;
    process->threads[0].rsp->rbx = 0;
    
    return 0;
}




int process_register_signal_setup(
        process_t* process, 
        void* function, 
        sighandler_t* table
) {
    // the table should be in userspace
    assert(!interrupt_enable());

    if(!is_user(table) || !is_user(function))
        return -1;

    process->sig_table = table;
    process->sig_return_function = function;

    return 0;    
}


// interrupts should be disabled
// process should be locked
static
int prepare_process_signal(process_t* process, int signal) {
    /**
     * 
     * This function does two things.
     * 1. it sets sig_current to the signal number to 
     * keep track of what signal is executing
     * 2. simulate a call to the signal handler.
     * 
     * While 1. is pretty easy, 2. is more tricky.
     * 
     * 2.: the idea is to prepare the stack to call:
     * - signal_handler_fun[signal](signal);
     * 
     * - put a return address on the stack,
     *   pointing to a cleanup function: sig_return_function
     *   that should invoke a SIGRETURN syscall, that will
     *   call process_end_of_signal(...).
     * 
     * to do that, we need to:
     * - save the context to sig_return_context (which will 
     *   be restored by process_end_of_signal)
     * 
     * - add the return address to the stack
     *      - move the stack context downward in the stack
     *        to put the return address
     * 
     *      - put the return address
     * 
     * - modify the context to simulate a function call to 
     *   the handler
     *      - set rip to the signal handler
     * 
     *      - change rsp to the match the moved stack context
     * 
     *      - set rdi to the argument of the handler
     * 
     * 
     * 
     * 
     * the job of process_end_of_signal is to:
     * - restore the context saved in sig_return_context (copy the 
     *   context at the right address and change rsp value)
     * 
     * - free the context
     * 
     * - clear sig_current
     * 
     */
    assert(!interrupt_enable());

    set_user_page_map(process->page_dir_paddr);


    process->sig_current = signal;

    
    // the signal should be pending
    assert((process->sig_pending & (1 << signal)) != 0);

    // disable pending bit
    process->sig_pending ^= (1 << signal);

    // @todo make sure thread0 is not running on any core
    assert(process->n_threads >= 1);
    assert(process->threads[0].state != RUNNING);

    // main thread
    thread_t* thread0 = &process->threads[0];

    assert(process->sig_return_context == NULL);


    process->sig_return_context = thread0->rsp;

    ////////////////////////////////////////////////
    //////////////// return address ////////////////
    ////////////////////////////////////////////////
    // set return address. to do that, we must 
    // move the context a bit higher in the stack

    void* user_rsp;

    if(thread0->syscall_user_rsp) {
        // the thread is currently in a system call. 
        // the current stack resides in kernel space

        user_rsp = thread0->syscall_user_rsp;
    }
    else {
        user_rsp = (void*)thread0->rsp->rsp;
    }


    assert(is_user(user_rsp));


    // allocate memory for return address
    user_rsp -= sizeof(void*);

    if(user_rsp < (void*)thread0->stack.base) {
        // stack overflow!
        log_warn("stackoverflow when signaling process %d", process->pid);

        return -1;
    }

    *(void**)user_rsp = process->sig_return_function;


    thread0->rsp --;

    if((void *)thread0->rsp < thread0->kernel_stack.base) {
        // kernel space stack overflow
        log_warn("kernel stack overflow");
        return -1;
    }



    thread0->rsp->rsp = (uint64_t)user_rsp;
    
    // don't execute the user code with CPL=1 :)
    thread0->rsp->cs = USER_CS;
    thread0->rsp->ss = USER_DS;
    thread0->rsp->rflags = USER_RF;

    // argument to function
    thread0->rsp->rdi = signal;


    // jump to signal handler
    assert(process->sig_table[signal]);
    thread0->rsp->rip = (uint64_t)process->sig_table[signal];


    // cancel the thread sleep if it is sleeping
    sleep_cancel(process->pid, process->threads[0].tid);
    
    // unblock the thread if blocked
    // sleep_cancel will unblock the sleep of
    // the thread if it is sleeping, but
    // we want to unblock it even if it is
    // not sleeping
    if(process->threads[0].state == BLOCKED)
        sched_unblock_thread(&process->threads[0]);

    return 0;
}


int process_handle_signal(process_t* process) {
    assert(!interrupt_enable());
    assert(process->lock);
    assert(process->n_threads > 0);
    
    if(process->sig_pending == 0)
        return -1;
    if(process->sig_current != NOSIG)
        return -1;
    if(process->threads[0].uninterruptible)
        return -1;

    // returns 1 + the index of the least significant 
    // non null bit of sig_pending
    int bit = __builtin_ffs(process->sig_pending);

    assert(bit);

    int which = bit - 1;

    return prepare_process_signal(process, which);
}



// defined in restore_context.s
void __attribute__((noreturn)) _restore_context(struct gp_regs* rsp);

// interrupts should be disabled
// process user memory should be mapped
int process_end_of_signal(process_t* process) {
    assert(!interrupt_enable());

    assert(get_user_page_map() == process->page_dir_paddr);    

    // map the right process memory
    //map_process_memory(process);

    // check that the process was indeed in a signal handler
    if(process->sig_current == NOSIG)
        return -1;

    assert(process->sig_return_context != NULL);




    process->threads[0].rsp = (void *)process->sig_return_context;

    process->sig_return_context = NULL;


    // the process is now back in a normal state:
    // clear the sig_current field
    process->sig_current = NOSIG;

    // don't return from the system call,
    // directly jump to the code
//    _restore_context(process->threads[0].rsp);
//
//    __builtin_unreachable();
    return 0;
}


// this function also releases the process lock
static
void unblock_sigwait_threads_and_release(process_t* process, uint64_t rflags) {

    // construct a list of threads to unblock
    unsigned n = 0;

    int* tids = malloc(sizeof(int) * process->n_threads);


    for(unsigned i = 0; i < process->n_threads; i++) {
        if(process->threads[i].sig_wait) {
            process->threads[i].sig_wait = 0;

            tids[n++] = process->threads[i].tid;
        }
    }


    unsigned pid = process->pid;

    // release the process
    spinlock_release(&process->lock);
    set_rflags(rflags);


    // actually unblock
    // it can only be done without the process lock
    for(unsigned i = 0; i < n; i++)
        sched_unblock(pid, tids[i]);



    free(tids);
}


int process_trigger_signal(pid_t pid, int signal) {

    uint64_t rf = get_rflags();

    // grab the process
    process_t* process = sched_get_process(pid);


    if(!process) {
        // no such process
        set_rflags(rf);
        return -1;
    }


    // map the process memory into lower half
    uint64_t saved_proc_page_map = get_user_page_map();
    set_user_page_map(process->page_dir_paddr);

    if(!process->sig_table) {
        // no signal table: signal ignored
    }
    else if(process->sig_table[signal] == SIG_IGN) {
        // signal ignored
    }
    else {
        assert(process->n_threads > 0);
        process->sig_pending |= (1 << signal);
    }

    // unblock threads that wait for a signal if necessary
    unblock_sigwait_threads_and_release(process, rf);

    // unblock the thread
    sleep_cancel (pid, 1);
    sched_unblock(pid, 1);

    // restore the process memory map
    set_user_page_map(saved_proc_page_map);

    return 0;
}


