#include "sched.h"
#include "process.h"
#include "../int/idt.h"
#include "../int/apic.h"
#include "../lib/logging.h"
#include "../memory/vmap.h"
#include "../memory/heap.h"
#include "../memory/paging.h"

/**
 * schedule algorithm:
 *  N ready queues of threads
 * 
 * ready_queue1: most priority queue
 *  ...
 * ready_queueN: least priority queue
 * 
 * 
 * choose_next_task:
 *   for i in[1 ... N]:
 *      t = ready_queue[i].pop()
 *      if t:
 *          up_tasks()
 *          return t
 *     return null
 * 
 * up_tasks
 *   for i in[2 ... N]:
 *      t = ready_queues[i].get()
 *      if t:
 *          ready_queues[i-1].put(t)
 * 
 */


// pid 

static pid_t current_pid = KERNEL_PID;
static tid_t current_tid = 0;

// 1 iif currenly in irq
static int currenly_in_irq = 0;


static size_t n_processes = 0;
static size_t n_threads = 0;

static process_t* processes;

/**
 * when invoking a syscall,
 * syscall_stacks[lapic_id] should contain
 * the stack pointer of the current thread
 * executing on the CPU with the given lapic_id
 */
void** syscall_stacks;


void sched_init(void) {
    // init syscall stacks

    syscall_stacks = malloc(sizeof(void*) * get_smp_count());
}

/**
 * @brief alloc a process in the processes list
 * and return a pointer on it
 * 
 * @return process_t* 
 */
static process_t* add_process(void) {
    // add it in the end

    unsigned id = n_processes++;
    processes = realloc(processes, sizeof(process_t) * n_processes);

    return processes + id;
}


#define N_QUEUES 4

// alloc pids in a circular way
static pid_t last_pid = KERNEL_PID;


// contains kernel regs if saved_context was called
// while current_pid = 0
gp_regs_t* kernel_saved_rsp = NULL;

// defined in restore_context.s
void __attribute__((noreturn)) _restore_context(struct gp_regs* rsp);


static 
thread_t* get_thread_by_tid(process_t* process, tid_t tid) {

    // sequencial research 
    for(unsigned i = 0; i < process->n_threads; i++) {

        if(process->threads[i].tid == tid)
            return &process->threads[i];

    }
    return NULL;
}


process_t* sched_get_process(pid_t pid) {
    
    // sequencial research
    for(unsigned i = 0; i < n_processes; i++) {
        if(processes[i].pid == pid)
            return &processes[i];
    }

    return NULL;
}





pid_t sched_create_process(pid_t ppid, const void* elffile, size_t elffile_sz) {
    process_t new;

    // NULL: kernel process
    process_t* parent = NULL;

    if(ppid != KERNEL_PID) {
        parent = sched_get_process(ppid);

        if(!parent) {
            // no such ppid
            return -1;
        }
    }
    
    if(!create_process(&new, parent, elffile, elffile_sz))
        return -1;
    
    _cli();

    // add it to the processes list
    *add_process() = new;

    _sti();

    return new.pid;
}




void  sched_save(gp_regs_t* rsp) {

    currenly_in_irq = 1;

    if(current_pid == KERNEL_PID) {
        kernel_saved_rsp = rsp;
    }
    else {
        kernel_saved_rsp = NULL;

        process_t* p = sched_get_process(current_pid);
        assert(p);
        thread_t* t = get_thread_by_tid(p, current_tid);

        assert(t);
        t->rsp = rsp;
    }
}


void schedule(void) {
    if(currenly_in_irq && current_pid == KERNEL_PID) {
        assert(kernel_saved_rsp);

        currenly_in_irq = 0;
        _restore_context(kernel_saved_rsp);
    }

    // do not schedule if the interrupted task
    // was the kernel

    // only execute the process with pid 1 for now.

    process_t* p = sched_get_process(1);
    thread_t* t = &p->threads[0];

    assert(p->n_threads == 1);
    assert(p->pid == 1);
    assert(t->tid == 1);


    // map the user space
    set_user_page_map(p->page_dir_paddr);

    _cli();

    current_pid = p->pid;
    current_tid = t->tid;

    syscall_stacks[get_lapic_id()] = (void*) t->kernel_stack.base + t->kernel_stack.size;

    _restore_context(t->rsp);
}


pid_t sched_current_pid(void) {
    return current_pid;
}
tid_t sched_current_tid(void) {
    return current_tid;
}

process_t* sched_current_process(void) {
    if(!current_pid) {
        // current process: kernel 
        return NULL;
    }
    process_t* process = sched_get_process(current_pid);

    assert(process);

    return process;
}


pid_t alloc_pid(void) {
    return ++last_pid;
}
