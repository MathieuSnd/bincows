#include <stdint.h>

#include "time.h"
#include "registers.h"
#include "string.h"

#include "../int/apic.h"
#include "../sched/sched.h"
#include "../sync/spinlock.h"


static unsigned n_sleeping_threads = 0;

typedef struct sleeping_thread {
    tid_t tid;
    pid_t pid;

    uint64_t wakeup_time;
} sleeping_thread_t;

/**
 * this array is used to store the sleeping threads
 * the array is sorted by the wakeup time
 * the first element is the thread that should be waken up
 * first
 */
static sleeping_thread_t* sleeping_threads = NULL;

fast_spinlock_t sleep_lock = {0};

static 
unsigned insert_position(uint64_t wakeup_time, tid_t tid, pid_t pid) {

    if(n_sleeping_threads == 0) {
        return 0;
    }

    // insert the tid in its correct position
    // the array is sorted so we can use a binary search
    // to find the correct position
    int A = 0;
    int B = n_sleeping_threads - 1;

    while(A <= B) {
        int mid = (A + B) / 2;

        int t = sleeping_threads[mid].wakeup_time;

        if(t > wakeup_time) {
            B = mid - 1;
        } else {
            A = mid + 1;
        }

        // correct position
        if(A > B)
            return mid;
    }

    panic("unreachable");
}


static
void register_sleeping_thread(tid_t tid, pid_t pid, uint64_t wakeup_time) {

    uint64_t rf = get_rflags();
    _cli();
    spinlock_acquire(&sleep_lock);

    sleeping_threads = realloc(
        sleeping_threads, 
        sizeof(sleeping_thread_t) * (n_sleeping_threads + 1)
    );

    

    int i = insert_position(wakeup_time, tid, pid);

    // shift the array to the right
    memmove(
        sleeping_threads + i + 1, 
        sleeping_threads + i, 
        sizeof(sleeping_thread_t) * (n_sleeping_threads - i)
    );

    // actually insert the thread

    sleeping_threads[i] = (sleeping_thread_t) {
        .tid=tid, 
        .pid=pid,
        .wakeup_time=wakeup_time,
    };


    
    n_sleeping_threads++;

    spinlock_release(&sleep_lock);
    set_rflags(rf);
}



void wakeup_threads(void) {
    uint64_t rf = get_rflags();
    _cli();
    spinlock_acquire(&sleep_lock);


    uint64_t now = clock_ns();

    // find the first thread that should not be waken up
    unsigned j = 0;

    for(;j < n_sleeping_threads && sleeping_threads[j].wakeup_time < now;)
        j++;

    if(j != 0) {
        // wake up the threads
        if(sched_is_running()) {
            for(int i = 0; i < j; i++) {
                sleeping_thread_t* t = sleeping_threads + i;
                
                sched_unblock(t->pid, t->tid);
            }
        }

        // remove the threads that have been waken up
        memmove(
            sleeping_threads,
            sleeping_threads + j,
            sizeof(sleeping_thread_t) * (n_sleeping_threads - j)
        );

        n_sleeping_threads -= j;
    }


    spinlock_release(&sleep_lock);
    set_rflags(rf);
}




void sleep(unsigned ms) {
    uint64_t begin = clock_ns();

    assert(interrupt_enable());

    uint64_t wakeup_time = begin + ms * 1000 * 1000;


    if(sched_is_running()) {

        // sleeping as the kernel process
        // means to yield the cpu if some job can be done,
        // or idle else
        if(sched_current_pid() == KERNEL_PID) {
            sched_kernel_wait(ms);
            return;
        }
            //sched_yield();

        //stacktrace_print();
        do {
            _cli();
            register_sleeping_thread(
                sched_current_tid(), 
                sched_current_pid(), 
                wakeup_time
            );
            sched_block();
            _sti();
            //sched_yield();
        }
        while (clock_ns() < wakeup_time);
    }
    else

        do 
            asm volatile("hlt");
        while (clock_ns() < wakeup_time);
}