#include "sched.h"
#include "process.h"
#include "../int/idt.h"
#include "../acpi/power.h"
#include "../int/apic.h"
#include "../lib/logging.h"
#include "../memory/vmap.h"
#include "../memory/heap.h"
#include "../memory/paging.h"
#include "../memory/physical_allocator.h"
#include "../drivers/block_cache.h"
#include "../memory/gdt.h"
#include "../smp/smp.h"

// for get_rflags()
#include "../lib/registers.h"
#include "../lib/panic.h"
#include "../lib/string.h"
#include "../lib/time.h"

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

// 1 iif currently in irq
static int currently_in_irq = 0;
static int currently_in_nested_irq = 0;

static int sched_running = 0;

// 1 if the kernel process is doing
// something important and doesn't 
// want to be interrupted
static _Atomic int kernel_process_running = 0;



int sched_is_running(void) {
    return sched_running;
}



static size_t n_processes = 0;
//static size_t n_threads = 0;

static process_t** processes = NULL;

// killed processes aren't freed instantly
// because freeing one involves interrupts
// to be enabled, and they can be killed 
// in an IRQ. As nested IRQs arn't 
// implemented, an iddle task takes care
// of the killed processes once in a while
static process_t** killed_processes = NULL;
static int n_killed_processes = 0;


// this field is useful for the kernel process
// to know if it can put the system in idle
static int n_ready_processes = 0;

/**
 * this lock is used to protect the process table
 * and the ready queues
 * 
 */
static fast_spinlock_t sched_lock = {0};

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


static void block_irq_handler(struct driver* unused) {
    (void) unused;

    process_t* p = sched_current_process();
    assert(p);

    thread_t* t = sched_get_thread_by_tid(p, current_tid);

    if(t->state == READY) {
        // the approch is optimistic
        // a processor could have executed 
        // the thread. This is why the block function
        // should be executed in a loop.

        // the thread will eventually block.

        // @todo it doesn't work if the thread
        // gets unblocked before it is actually blocked!!
        // it can be avoided with a "soft_blocked" flag
        t->state = BLOCKED;

        n_ready_processes--;
    }
    // else, don't do anything

    spinlock_release(&p->lock);

    schedule();
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
    processes = realloc(processes, sizeof(process_t*) * n_processes);
    

    return processes[id] = malloc(sizeof(process_t));
}




void sched_init(void) {
    // init syscall stacks
    syscall_stacks = malloc(sizeof(void*) * get_smp_count());

    // init the virtual yield IRQ 
    register_irq(YIELD_IRQ, yield_irq_handler, 0);
    register_irq(BLOCK_IRQ, block_irq_handler, 0);

    // add the kernel process
    _cli();
    // sched lock is taken there
    
    create_kernel_process(add_process());

    spinlock_release(&sched_lock);

    _sti();


    // unblock the kernel process
    sched_unblock(KERNEL_PID, 1);
}


void sched_start(void) {
    sched_running = 1;
    schedule();
}


#define SIGCHLD 17


static int add_killed_process(process_t* process) {
    assert(process->n_threads == 0);
    assert(sched_lock.val == 1);
    assert(!interrupt_enable());
    

    // add the process in the killed processes list
    killed_processes = realloc(
            killed_processes, 
            sizeof(process_t*) * (n_killed_processes + 1)
    );

    killed_processes[n_killed_processes++] = process;

    return 0;
} 


static int remove_process(pid_t pid) {
    // first lock the sched lock
    spinlock_acquire(&sched_lock);



    // remove it from the list

    int found = 0;

    pid_t ppid = 0;

    // sequential search
    for(unsigned i = 0; i < n_processes; i++) {
        if(processes[i]->pid == pid) {
            
            // free it
            spinlock_acquire(&processes[i]->lock);

            ppid = processes[i]->ppid;

            // copy it to free it later on
            add_killed_process(processes[i]);

            // remove it
            processes[i] = processes[--n_processes];

            found = 1;
            break;
        }
    }

    // unlock the sched lock
    spinlock_release(&sched_lock);


    // signal the parent process that a child has died
    // if it exists
    process_trigger_signal(ppid, SIGCHLD);

    return found;

}


#define N_QUEUES 4


// contains kernel regs if saved_context was called
// while current_pid = 0
gp_regs_t* kernel_saved_rsp = NULL;

// defined in restore_context.s
void __attribute__((noreturn)) _restore_context(struct gp_regs* rsp);


thread_t* sched_get_thread_by_tid(process_t* process, tid_t tid) {

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

    // not found
    process_t* process = NULL;

    uint64_t rf = get_rflags();

    _cli();


    // lock the process table
    spinlock_acquire(&sched_lock);

    // sequencial research
    for(unsigned i = 0; i < n_processes; i++) {
        if(processes[i]->pid == pid)
            process = processes[i];
    }

    if(!process) {
        // unlock the process table
        spinlock_release(&sched_lock);

        // log_debug("process %d not found, over %d processes", pid, n_processes);
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



pid_t sched_create_process(pid_t ppid, const void* elffile, 
                size_t elffile_sz, fd_mask_t fd_mask) {
    process_t new;

    // NULL: no parent
    process_t* parent;


    uint64_t rf = get_rflags();
    _cli();

    parent = sched_get_process(ppid);

    if(!parent) {
        // no such ppid
        set_rflags(rf);
        log_debug("no such ppid %d", ppid);
        return -1;
    }

    int ret = create_process(&new, parent, elffile, elffile_sz, fd_mask);

    // unlock process 

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


void sched_launch_process(process_t* p) {
    assert(p->n_threads == 1);
    p->threads[0].state = READY;
    n_ready_processes++;
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
            n_ready_processes--;


            // remove the element from the list
            *thread = process->threads[process->n_threads - 1];
            process->n_threads--;

            if(process->n_threads == 0)
                free(process->threads);

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
    currently_in_nested_irq += currently_in_irq;
    currently_in_irq = 1;

    //assert(!currently_in_nested_irq);

    //log_info("sched_save rsp = %lx", rsp);

    if(!sched_running) {
        kernel_saved_rsp = rsp;
    }
    else {
        kernel_saved_rsp = NULL;

        // p's lock is taken there
        process_t* p = sched_get_process(current_pid);
        assert(p);
        thread_t* t = sched_get_thread_by_tid(p, current_tid);

        assert(t);

        n_ready_processes++;
        t->state = READY;
        t->rsp = rsp;

        // save page table
        p->saved_page_dir_paddr = get_user_page_map();

        // release process lock
        spinlock_release(&p->lock);

    }
}



gp_regs_t* sched_saved_context(void) {
    assert(currently_in_irq);
    assert(!interrupt_enable());

    if(!sched_running) {
        return kernel_saved_rsp;
    }

    process_t* p = sched_get_process(current_pid);
    assert(p);
    thread_t* t = sched_get_thread_by_tid(p, current_tid);
    assert(t);
    gp_regs_t* context = t->rsp;

    // release process lock
    spinlock_release(&p->lock);

    // here, context is assured to be valid
    // until the end of the interrupt handler


    return context;
}



void sched_free_killed_processes(void) {
    assert(!currently_in_nested_irq);
    
    if(n_killed_processes == 0)
        return;

    _cli();
    spinlock_acquire(&sched_lock);

    // CS
    // copy the killed processes list to a local variable
    // then clear the list

    unsigned n = n_killed_processes;

    process_t** to_kill = malloc(sizeof(process_t*) * n);
    memcpy(
        to_kill,
        killed_processes, 
        sizeof(process_t*) * n
    );

    free(killed_processes);

    killed_processes = NULL;

    n_killed_processes = 0;
    spinlock_release(&sched_lock);

    // CS end
    _sti();



    for(unsigned i = 0; i < n; i++) {
        process_t* p = to_kill[i];

        // free the process
        free_process(p);
    }
    free(to_kill);
}


void sched_cleanup(void) { 
    _cli();

    sched_running = 0;

    // @todo: invoke sleep IPIs to 
    // other CPUs

    // start by setting current pid to 0
    current_pid = KERNEL_PID;

    sched_free_killed_processes();


    spinlock_acquire(&sched_lock);

    cleanup_sleeping_threads();

    for(unsigned i = 0; i < n_processes; i++) {
        process_t* p = processes[i];

        spinlock_acquire(&p->lock);

        for(unsigned j = 0; j < p->n_threads; j++) {
            thread_t* t = &p->threads[j];

            thread_terminate(t, 1);
            n_ready_processes--;
        }

        p->n_threads = 0;

        // @todo do better
        // this is bad
        //
        // well maybe it isn't so bad
        // since we disabled the scheduler
        // schedule() won't ever be called
        // it is quite safe to do the following 
        // I guess
        _sti();
        free_process(p);
        _cli();
    }

    if(n_processes)
        free(processes);
    if(syscall_stacks)
        free(syscall_stacks);

    n_processes = 0;


    spinlock_release(&sched_lock);
    _sti();
}


void sched_yield(void) {   
    // invoke a custom interrupt
    // that does nothing but a
    // rescheduling
    asm volatile("int $" XSTR(YIELD_IRQ));
}


static process_t* choose_next(void) {
    // Round Robin (for now)
    static unsigned current_process = 0;

    // chosen process
    volatile process_t* p = NULL;

    _cli();

    assert(n_processes);

    // lock the process table
    spinlock_acquire(&sched_lock);

    if(current_process >= n_processes)
        current_process = 0;
    

    unsigned i = current_process;
    
    while(1) {
        p = (volatile process_t*)processes[i];

        if(   p->n_threads 
           && p->threads[0].state == READY
        ) {
            // we found a process to schedule
            break;
        }

        i = (i + 1) % n_processes;


        if(i == current_process) {
            // we have looped around
            // and haven't found a process
            // to schedule
            asm volatile("hlt");
            //panic("no process to schedule");
        }
    }

    // log_info("choosing process %d", p->pid);



    assert(p->threads[0].state == READY);

    // lock the process
    spinlock_acquire(&p->lock);

    current_process = i + 1;


    // unlock the process table
    spinlock_release(&sched_lock);

    assert(is_in_heap((void*)p));
    assert(is_in_heap(p->threads));


    // no process found
    return (process_t*)p;
}

#define RR_TIME_SLICE_MS 10


int timer_irq_should_schedule(void) {

    // round robin
    static uint64_t c = 0;

    assert(!currently_in_nested_irq);

    // don't schedule if we are in an irq
    if(currently_in_nested_irq || !sched_running)
        return 0;

    // don't schedule if the kernel process
    // is in its critical section
    if(kernel_process_running)
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
    assert(currently_in_irq);

    if(currently_in_nested_irq)
        currently_in_nested_irq--;
    else {
        currently_in_irq = 0;
    }
}



void sched_irq_ack(void) {
    assert(currently_in_irq);
    assert(!currently_in_nested_irq);
    _cli();

    if(!currently_in_nested_irq && sched_running) {
        process_t* p = sched_current_process();
        thread_t* t = sched_get_thread_by_tid(p, current_tid);

        if(!t) {
            // no process or no thread
            schedule();
        }   

        n_ready_processes--;
        t->state = RUNNING;
        
        // release process lock
        spinlock_release(&p->lock);
    }

    irq_end();
}



void sched_block(void) {

    assert(!currently_in_irq);

    if(!sched_running) {
        asm volatile("hlt");
        return;
    }

    // the kernel process must not be blocked
    // because it the idle process
    assert(current_pid != KERNEL_PID);

    asm volatile("int $" XSTR(BLOCK_IRQ));    
}



void sched_kernel_wait(uint64_t ns) {
    // for now, only the kernel process does that
    assert(current_pid == KERNEL_PID);


    uint64_t sleep_begin = clock_ns();
    
    while(clock_ns() < sleep_begin + ns) {
        assert(n_ready_processes >= 0);
        if(n_ready_processes == 0) {
            // we are the only ready process
            asm volatile("hlt");
        }
        else {
            // there is at least someone else,
            // let him runs
            sched_yield();
        }
        // spin
    }
}



void sched_unblock_thread(thread_t* t) {

    if(t->state != BLOCKED) {
        //log_info("unblocking unblocked thread");
        return;
    }

    t->state = READY;

    n_ready_processes++;
}

void sched_unblock(pid_t pid, tid_t tid) {
    // at initialization, this function is called
    // to unblock the kernel process
    // BEFORE the scheduler is running
    //if(!sched_running) {
    //    return;
    //}

    //log_info("unblocking %d:%d %d", pid, tid,n_ready_processes);

    uint64_t rf = get_rflags();
    _cli();
    process_t* p = sched_get_process(pid);
    
    
    //assert(p);

    if(!p)
        return;

    thread_t* t = sched_get_thread_by_tid(p, tid);
    
    //assert(t);

    sched_unblock_thread(t);


    spinlock_release(&p->lock);
    set_rflags(rf);
}



void schedule(void) {

    // disable interrupts
    _cli();

    
    assert(!kernel_process_running);

    assert(sched_running);


/*
    if(currently_in_irq && current_pid == KERNEL_PID) {
        // do not schedule if the interrupted task
        // was the kernel
        assert(kernel_saved_rsp);

        irq_end();
        _restore_context(kernel_saved_rsp);
    }
*/
    //log_info("schedule");

    assert(!currently_in_nested_irq);

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
            n_ready_processes--;
            
            // remove the element from the list
            *t = p->threads[p->n_threads - 1];
            p->n_threads--;

            if(p->n_threads == 0) {
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



    void* kernel_sp;

    if(t->tid == 1 && p->sig_current != NOSIG) {
        // currently, a signal is being handled
        // it means that a context is saved in the kernel
        // stack. we should then go bellow the saved context
        assert(p->sig_return_context);
        kernel_sp = p->sig_return_context;
    }
    else {
        // kernel stack is completely empty
        kernel_sp = t->kernel_stack.base + t->kernel_stack.size;
        assert(kernel_sp);

    }


    if(p->pid != current_pid || t->tid != current_tid) {
        // we should change the IRQ stack
        set_tss_rsp((uint64_t)kernel_sp);
    }

    assert(t->state == READY);

    t->state = RUNNING;
    n_ready_processes--;

    
    current_pid = p->pid;
    current_tid = t->tid;


    // unlock the process
    // there is no risk for the process to be freed
    // because the current thread is marked RUNNING
    spinlock_release(&p->lock);

    syscall_stacks[get_lapic_id()] = kernel_sp;

    if(!(t->rsp->cs == USER_CS || t->rsp->cs == KERNEL_CS)) {
        log_warn("wrong cs: %x", t->rsp->cs);
        panic("wrong cs");
    }
    if(!(t->rsp->ss == USER_DS || t->rsp->ss == KERNEL_DS)) {
        log_warn("wrong ss: %x", t->rsp->ss);
        stacktrace_print();
        panic("wrong ss");
    }

    if(currently_in_irq)
        irq_end();


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
    process_t* process = sched_get_process(current_pid);

    assert(process);

    return process;
}


int lazy_shutdown = 0;
void* const bs_stack_end;


/**
 * this function is called by the kernel process
 * to engage the shutdown/reboot  sequence.
 * 
 * @param reboot: if non 0, the system will reboot
 * else, it will shutdown
 */
static void kernel_process_shutdown(int do_reboot) {
    // first need to suicide this process

    _cli();            
    process_t* p = sched_get_process(current_pid);

    assert(current_pid == KERNEL_PID);
    assert(p->n_threads == 1);
    thread_t* t = &p->threads[0];

    t->state = READY;

    // release p lock
    spinlock_release(&p->lock);

    // change the stack
    // and shutdown
    if(do_reboot)
        _change_stack(bs_stack_end, reboot);
    else
        _change_stack(bs_stack_end, shutdown);

    panic("unreachable");
}


// in permile
#define RECLAIM_MEMORY_THRESHOLD 100


//static 
void memory_check(void) {
    uint64_t total = total_pages();
    uint64_t available = available_pages();


    if(available * 1000 / total < RECLAIM_MEMORY_THRESHOLD) {
        

        // reclaim memory from cache
        int unit = available * 100 / total;
        int tenth = available * 1000 / total - 10 * unit; 
        log_info("critical amount of available memory: %u.%u percent.", unit, tenth);
        log_info("reclaiming memory from block caches.");
        block_cache_reclaim_pages(total);

        total = total_pages();
        available = available_pages();

        unit = available * 100 / total;
        tenth = available * 1000 / total - 10 * unit; 

        log_info("available memory after reclaim: %u.%u percent.", unit, tenth);
    }

}



void kernel_process_entry(void) {

    assert(current_pid == KERNEL_PID);
    assert(!currently_in_irq);
    assert(interrupt_enable());

    kernel_process_running = 0;

    for(;;) {
        
        if(n_killed_processes) {
            sched_free_killed_processes();
        }

        assert(!kernel_process_running);

        vfs_lazy_flush();

        // check if reclaiming memory is needed
        memory_check();

        
        if(lazy_shutdown) {
            kernel_process_running = 1;

            kernel_process_shutdown(0);
        }


        const uint64_t sleep_time = 200*1000*1000; // 200ms

        sched_kernel_wait(sleep_time);
    }
}



// assert that MAX_PID is a power of 2 - 1
static_assert((MAX_PID & (MAX_PID + 1)) == 0);

pid_t alloc_pid(void) {

    // alloc pids in a circular way
    static _Atomic pid_t last_pid = KERNEL_PID;

    return ++last_pid & (MAX_PID);
}
