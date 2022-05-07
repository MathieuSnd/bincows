#include "sched.h"
#include "process.h"
#include "../int/idt.h"
#include "../int/apic.h"
#include "../lib/logging.h"
#include "../memory/vmap.h"
#include "../memory/heap.h"
#include "../memory/paging.h"
#include "../memory/gdt.h"
#include "../smp/smp.h"

// for get_rflags()
#include "../lib/registers.h"
#include "../lib/panic.h"
#include "../lib/string.h"

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
static int currenly_in_nested_irq = 0;



static size_t n_processes = 0;
//static size_t n_threads = 0;

static process_t* processes;

// killed processes aren't freed instantly
// because freeing one involves interrupts
// to be enabled, and they can be killed 
// in an IRQ. As nested IRQs arn't 
// implemented, an iddle task takes care
// of the killed processes once in a while
static process_t* killed_processes = NULL;
static int n_killed_processes = 0;


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
    //log_info("YIELD current pid: %u", current_pid);
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


static int add_killed_process(process_t* process) {
    assert(process->n_threads == 0);
    assert(sched_lock.val == 1);
    assert(!interrupt_enable());
    

    // add the process in the killed processes list
    killed_processes = realloc(
            killed_processes, 
            sizeof(process_t) * (n_killed_processes + 1)
    );

    memcpy(
        killed_processes + n_killed_processes,
        process, 
        sizeof(process_t)
    );

    return 0;
} 


static int remove_process(pid_t pid) {
    // first lock the sched lock
    spinlock_acquire(&sched_lock);

    // remove it from the list

    int found = 0;

    // sequential search
    for(unsigned i = 0; i < n_processes; i++) {
        if(processes[i].pid == pid) {
            
            // free it
            spinlock_acquire(&processes[i].lock);

            // copy it to free it later on
            add_killed_process(&processes[i]);

            // remove it
            processes[i] = processes[--n_processes];

            found = 1;
            break;
        }
    }

    // unlock the sched lock
    spinlock_release(&sched_lock);

    return found;

    //log_warn("process %d removed", pid);
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

        pid_t pid = process->pid;
        spinlock_release(&process->lock);
        
        remove_process(pid);



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
    currenly_in_nested_irq += currenly_in_irq;
    currenly_in_irq = 1;

    assert(!currenly_in_nested_irq);

    //log_info("sched_save rsp = %lx", rsp);

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

        // save page table
        p->saved_page_dir_paddr = get_user_page_map();

        // release process lock
        spinlock_release(&p->lock);

    }
}


void sched_yield(void) {   
    // invoke a custom interrupt
    // that does nothing but a
    // rescheduling
    // log_warn("yield");
    
    /*
    _cli();

    // takes p's lock
    process_t* p = sched_get_process(current_pid);

    assert(p); // no such process

    thread_t* t = get_thread_by_tid(p, current_tid);

    assert(t); // no such thread

    //t->state = READY;

    // release process lock
    spinlock_release(&p->lock);
    */

    asm volatile("int $" XSTR(YIELD_IRQ));
}


static process_t* choose_next(void) {
    // Round Robin (for now)
    static unsigned current_process = 0;

    // chosen process
    process_t* p = NULL;

    _cli();

    // lock the process table
    spinlock_acquire(&sched_lock);

    if(current_process >= n_processes)
        current_process = 0;
    

    if(n_processes) {
        p = &processes[current_process];
        // lock the process
        // log_debug("choose_next: %u / %u", current_process, n_processes);
        spinlock_acquire(&p->lock);

        current_process++;
    }

    // unlock the process table
    spinlock_release(&sched_lock);




    // no process found
    return p;
}

#define RR_TIME_SLICE_MS 20


int timer_irq_should_schedule(void) {
    // round robin
    static uint64_t c = 0;

    // don't schedule if we are in an irq
    if(currenly_in_nested_irq)
        return 0;

    // LAPIC_IRQ_FREQ is in Hz
    if(c++ == RR_TIME_SLICE_MS * LAPIC_IRQ_FREQ / 1000) {
        c = 0;
        return 1;
    }
    return 0;
}


static void irq_end(void) {

    assert(!interrupt_enable());
    assert(currenly_in_irq);

    if(currenly_in_nested_irq)
        currenly_in_nested_irq--;
    else {
        currenly_in_irq = 0;
    }
}

void sched_irq_ack(void) {
    assert(currenly_in_irq);
    
    _cli();

    if(!currenly_in_nested_irq) {
        process_t* p = sched_current_process();
        thread_t* t = get_thread_by_tid(p, current_tid);

        if(!t) {
            // no process or no thread
            schedule();
        }

        t->state = RUNNING;
        
        // release process lock
        spinlock_release(&p->lock);
    }

    irq_end();
}

void schedule(void) {

    // disable interrupts
    _cli();

    assert(!currenly_in_nested_irq);

    if(currenly_in_irq && current_pid == KERNEL_PID) {
        // do not schedule if the interrupted task
        // was the kernel
        assert(kernel_saved_rsp);

        irq_end();
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
        assert(t->state != RUNNING);
        assert(t->state == READY);


        if(t->should_exit && t->state == READY) {
            log_debug("terminating process %u", p->pid);
            // if t->state != READY,
            // the process might be executing already 
            // in kernel space, on the core we are executing 
            // right now. Thus, we can't kill it, as 
            // our kernel stack resides in its memory.

            // lazy thread kill
            // we must kill the thread
            // and free it
            // lock the process

            thread_terminate(t, t->exit_status);

            if(--p->n_threads == 0) {
                // no more threads, we can kill the process

                // remove the process from the list
                // and free it

                // to avoid deadlocks, we must release 
                // the process and retake it
                int pid = p->pid;
                spinlock_release(&p->lock);

                remove_process(pid);

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

    // only flush tlb if the process changed
    if(p->pid != current_pid)
        set_user_page_map(p->saved_page_dir_paddr);

    if(p->pid != current_pid || t->tid != current_tid) {
        // we should change the IRQ stack
        set_tss_rsp(t->kernel_stack.base + t->kernel_stack.size);
    }

    assert(t->state == READY);

    t->state = RUNNING;

    
    current_pid = p->pid;
    current_tid = t->tid;


    // unlock the process
    // there is no risk for the process to be freed
    // because the current thread is marked RUNNING
    spinlock_release(&p->lock);

    syscall_stacks[get_lapic_id()] = (void*) t->kernel_stack.base + t->kernel_stack.size;

    if(!(t->rsp->cs == USER_CS || t->rsp->cs == KERNEL_CS)) {
        log_warn("wrong cs: %x", t->rsp->cs);
        panic("wrong cs");
    }
    if(!(t->rsp->ss == USER_DS || t->rsp->ss == KERNEL_DS)) {
        log_warn("wrong ss: %x", t->rsp->ss);
        stacktrace_print();
        panic("wrong ss");
    }

    if(currenly_in_irq)
        irq_end();

    //log_warn("pid = %u, rax = %x, rsp = %lx", p->pid, t->rsp->rax, t->rsp);

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
