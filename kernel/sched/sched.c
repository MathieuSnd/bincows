#include "sched.h"
#include "process.h"
#include "../int/idt.h"
#include "../int/apic.h"
#include "../lib/logging.h"
#include "../memory/vmap.h"
#include "../memory/heap.h"
#include "../memory/paging.h"
#include "../smp/smp.h"

// for get_rflags()
#include "../lib/registers.h"

// for the yield interrupt handler
#include "../int/irq.h"
//#include "../"

#define XSTR(s) STR(s)
#define STR(x) #x

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
 * this lock is used to protect the process table
 * and the ready queues
 * 
 */
fast_spinlock_t sched_lock = {0};

/**
 * when invoking a syscall,
 * syscall_stacks[lapic_id] should contain
 * the stack pointer of the current thread
 * executing on the CPU with the given lapic_id
 */
void** syscall_stacks;



static void yield_irq_handler(struct driver* unused) {
    (void) unused;

    schedule();
}


void sched_init(void) {
    // init syscall stacks
    syscall_stacks = malloc(sizeof(void*) * get_smp_count());

    // init the virtual yield IRQ 
    register_irq(YIELD_IRQ, yield_irq_handler, 0);
}


/**
 * @brief alloc a process in the processes list
 * and return a pointer on it.
 * The caller should have the interrupts disabled
 * 
 * the scheduler lock is aquired and not released
 * the caller is responsible for releasing the lock
 * after inserting the process in the list
 * 
 * @return process_t* 
 */
static process_t* add_process(void) {
    spinlock_acquire(&sched_lock);

    unsigned id = n_processes++;
    processes = realloc(processes, sizeof(process_t) * n_processes);

    //log_debug("add_process: %u", id);

    return processes + id;
}


static void remove_process(process_t* process) {
    // first lock the sched lock
    spinlock_acquire(&sched_lock);

    // remove it from the list

    spinlock_acquire(&process->lock);

    unsigned id = (process - processes);
    processes[id] = processes[--n_processes];

    spinlock_release(&processes[n_processes].lock);

    // unlock the sched lock
    spinlock_release(&sched_lock);


    //log_warn("process %d removed", id);
}


#define N_QUEUES 4

// alloc pids in a circular way
static pid_t last_pid = KERNEL_PID;


// contains kernel regs if saved_context was called
// while current_pid = 0
gp_regs_t* kernel_saved_rsp = NULL;

// defined in restore_context.s
void __attribute__((noreturn)) _restore_context(struct gp_regs* rsp);


/**
 * the process lock should be taken before 
 * calling this function
 * 
 * if process == NULL or the tid is not found, 
 * then return NULL
 * 
 */
static 
thread_t* get_thread_by_tid(process_t* process, tid_t tid) {

    if(!process)
        return NULL;

    // sequencial research 
    for(unsigned i = 0; i < process->n_threads; i++) {

        if(process->threads[i].tid == tid)
            return &process->threads[i];

    }
    return NULL;
}


process_t* sched_get_process(pid_t pid) {

    if(pid == KERNEL_PID)
        return NULL;

    // not found
    process_t* process = NULL;

    uint64_t rf = get_rflags();

    _cli();


    // lock the process table
    spinlock_acquire(&sched_lock);

    // sequencial research
    for(unsigned i = 0; i < n_processes; i++) {
        if(processes[i].pid == pid)
            process = &processes[i];
    }

    if(!process) {
        // unlock the process table
        spinlock_release(&sched_lock);

        log_debug("process %d not found, over %d processes", pid, n_processes);
        // restore rflags
        set_rflags(rf);

        return NULL;
    }

    // lock the process to make sure it won't be freed
    spinlock_acquire(&process->lock);

    // unlock the process table
    spinlock_release(&sched_lock);


    return process;
}





pid_t sched_create_process(pid_t ppid, const void* elffile, size_t elffile_sz) {
    process_t new;

    // NULL: kernel process
    process_t* parent = NULL;


    uint64_t rf = get_rflags();
    _cli();

    if(ppid != KERNEL_PID) {
        parent = sched_get_process(ppid);

        if(!parent) {
            // no such ppid

            // unlock process

            set_rflags(rf);
            log_debug("no such ppid %d", ppid);
            return -1;
        }
    }

    int ret = create_process(&new, parent, elffile, elffile_sz);

    // unlock process 

    if(parent)
        spinlock_release(&parent->lock);

    if(!ret) {
        log_debug("create_process failed");
        // error
        set_rflags(rf);
        return -1;
    }
    

    // add it to the processes list
    *add_process() = new;
    spinlock_release(&sched_lock);

    set_rflags(rf);

    return new.pid;
}


int sched_kill_process(pid_t pid, int status) {
    // the entier operation is critical
    _cli();

    // process is aquired there
    process_t* process = sched_get_process(pid);
    

    if(!process) {
        // no such process
        _sti();
        return -1;
    }

    int running = 0;

    for(unsigned i = 0; i < process->n_threads; i++) {
        thread_t* thread = &process->threads[i];

        if(thread->state == RUNNING) {
            // we can't kill a running process
            // we must wait for it to finish

            // we do a lazy kill

            // @todo send an IPI to the thread cpu
            // to force it to stop earlier
            
            thread->should_exit = 1;
            running = 1;
            log_debug("lazy kill: %d", thread->tid);
        }
        else if(thread->state == READY) {
            // remove it from the ready queue
            // and free the thread

            thread_terminate(thread, status);
            log_debug("thread %d terminated", thread->tid);
        }
    }



    if(!running) {
        // no running thread, we can kill the process

        // remove the process from the list
        // and free it

        // avoid deadlocks:
        // we must release the process lock 
        // before taking the sched lock
        // as a process could be concurrently
        // try to take the sched lock
        // and then try to take the process lock

        spinlock_release(&process->lock);
        
        remove_process(process);
        free_process(process);

        // the process has been successfully removed
        // from the list. Thus, noone can fetch it.
        // though it is still be taken
        //spinlock_acquire(&sched_lock);



    }
    else {
        // we can't kill the process
        // we must wait for it to finish
        spinlock_release(&process->lock);
    }

    _sti();

    // success
    return 0;
}


void  sched_save(gp_regs_t* rsp) {

    currenly_in_irq = 1;

    if(current_pid == KERNEL_PID) {
        kernel_saved_rsp = rsp;
    }
    else {
        kernel_saved_rsp = NULL;

        // p's lock is taken there
        process_t* p = sched_get_process(current_pid);
        assert(p);
        thread_t* t = get_thread_by_tid(p, current_tid);

        assert(t);

        t->state = READY;
        t->rsp = rsp;

        static int i = 0;

        // save page table
        p->saved_page_dir_paddr = get_user_page_map();

        // release process lock
        spinlock_release(&p->lock);

/*
        log_warn("SAVED cs=%x, ss=%x, rsp=%x, rip=%x", t->rsp->cs, t->rsp->ss, t->rsp->rsp, t->rsp->rip);
        stacktrace_print();
*/
    }
}


void sched_yield(void) {   
    // invoke a custom interrupt
    // that does nothing but a
    // rescheduling
    
    _cli();

    // takes p's lock
    process_t* p = sched_get_process(current_pid);

    assert(p); // no such process

    thread_t* t = get_thread_by_tid(p, current_tid);

    assert(t); // no such thread

    t->state = READY;

    // release process lock
    spinlock_release(&p->lock);

    asm volatile("int $" XSTR(YIELD_IRQ));
}


static process_t* choose_next(void) {

    // lock the process table
    spinlock_acquire(&sched_lock);

    // sequencial research
    for(int i = n_processes-1; i >= 0; i--) {
        process_t* process = &processes[i];

        if(process->n_threads > 0) {
            // lock the process to make sure it won't be freed
            spinlock_acquire(&process->lock);

            // unlock the process table
            spinlock_release(&sched_lock);

            // return it
            return process;
        }
    }

    // unlock the process table
    spinlock_release(&sched_lock);

    // no process found
    return NULL;
}

#define RR_TIME_SLICE_MS 20


int timer_irq_should_schedule(void) {
    // round robin
    static uint64_t c = 0;

    // LAPIC_IRQ_FREQ is in Hz
    if(c++ == RR_TIME_SLICE_MS * LAPIC_IRQ_FREQ / 1000) {
        c = 0;
        return 1;
    }
    return 0;
}

void sched_irq_ack(void) {
    assert(currenly_in_irq);
    
    _cli();

    process_t* p = sched_current_process();
    thread_t* t = get_thread_by_tid(p, current_tid);

    if(!t) {
        // no process or no thread
        schedule();
    }
    currenly_in_irq = 0;

    t->state = RUNNING;

    
    // release process lock
    spinlock_release(&p->lock);
}

void schedule(void) {

    // disable interrupts
    _cli();

    if(currenly_in_irq && current_pid == KERNEL_PID) {
        // do not schedule if the interrupted task
        // was the kernel
        assert(kernel_saved_rsp);

        currenly_in_irq = 0;
        _restore_context(kernel_saved_rsp);
    }

    // next process
    process_t* p;

    // next thread
    thread_t* t;

    
    // select one process
    while(1) {
        // lazy kill: we check if the process
        // should be scheduled.
        
        // process is locked there
        p = choose_next();
        t = &p->threads[0];

        assert(p->n_threads == 1);
        //assert(p->pid == 1);
        assert(t->tid == 1);


        if(t->should_exit && t->state == READY) {
            // if t->state != READY,
            // the process might be executing already 
            // in kernel space, on the core we are executing 
            // right now. Thus, we can't kill it, as 
            // our kernel stack resides in its memory.

            // lazy thread kill
            // we must kill the thread
            // and free it

            thread_terminate(t, t->exit_status);

            if(--p->n_threads == 0) {
                // no more threads, we can kill the process

                // remove the process from the list
                // and free it
                free_process(p);
            }
            else {
                // we can't kill the process
                // we must wait for it to finish
                spinlock_release(&p->lock);
            }
        }
        else break;
    }

    // the process is selected for execution

    // map the user space
    set_user_page_map(p->saved_page_dir_paddr);

    t->state = RUNNING;

    
    current_pid = p->pid;
    current_tid = t->tid;


    // unlock the process
    // there is no risk for the process to be freed
    // because the current thread is marked RUNNING
    spinlock_release(&p->lock);

    syscall_stacks[get_lapic_id()] = (void*) t->kernel_stack.base + t->kernel_stack.size;


    _restore_context(t->rsp);
}



void schedule_irq_handler(void) {
    schedule();
    panic("unreachable");
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
