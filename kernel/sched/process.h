#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../lib/elf/elf.h"

#include "../fs/vfs.h"

#include "signal/signal.h"
#include "../sync/spinlock.h"

// this file describes what are processes

typedef int pid_t;
typedef int32_t tid_t;

#ifndef SHMID_T
#define SHMID_T
typedef int shmid_t;
#endif

typedef unsigned fd_t;
struct shm_instance;


#define MAX_FDS 32
typedef uint32_t fd_mask_t;

#define FD_NONE 0
#define FD_FILE 1
#define FD_DIR 2

typedef struct file_descriptor {
    /*
     either one of these
        - FD_NONE
        - FD_FILE 
        - FD_DIR
     */
    int type; 

    union {
        file_handle_t* file;

        struct {
            struct DIR* dir;
            /**
             * current byte offset of the directory stream
             */
            size_t dir_boff;
        };
    };  
} file_descriptor_t;


// process wide user mapping descriptor
struct process_mem_map {
    // page diectory for the private process
    // memory. For now, represents a 512 GB PML4 
    // page entry, to the [0 -> 512 GB] memory range:
    // 1st PML4 entry
    uint64_t private_pd;

    // page diectory for the sharedprocess
    // memory. For now, represents a 512 GB PML4 
    // page entry, to the [1024 -> 1536 GB] memory range:
    // 3rd PML4 entry
    uint64_t shared_pd;
};

struct futex_waiter {
    void* uaddr;
    tid_t tid;
};


typedef struct process {
    pid_t pid;

    pid_t ppid;

    // memory map structure.
    // it should be passed to process_map()
    // and process_unmap().
    struct process_mem_map mem_map;

    uint64_t page_dir_paddr;

    size_t n_threads;
    struct thread* threads;

    // lock for the process and
    // threads
    // The lock does not have to be taken
    // to access fields in the process
    // in a kernel task associated with
    // a subsequent thread that is running
    spinlock_t lock;


    elf_program_t* program;

    // the instant of the creation of the
    // process
    uint64_t clock_begin;


    void* heap_begin;
    void* brk;

    // brk as seen by processes
    void* unaligned_brk;


    char* cwd;

    
    // size [MAX_FDS]
    file_descriptor_t* fds;

    /////////////////////
    /// signal fields ///
    /////////////////////

    // user signal handler
    void* sighandler;

    /**
     * bitmask of ignored signals.
     * If ignored, a signal shouldn't set
     * the corresponding sig_pending bit
     */
    sigmask_t ign_mask;


    /**
     * bitmask of blocked signals.
     * when blocked, a corresponding sig_pending
     * bit shouldn't be cleared and no handler should be
     * called.
     */
    sigmask_t blk_mask;


    // bitmask of pending signals
    // if at least one bit is set, then
    // thread 0 should be scheduled to handle the
    // highest priority signal no matter his state
    // (BLOCKED, RUNNING, etc)
    // then, a system call is used to clear the mask bit
    uint32_t sig_pending;

    // NOSIG if no signal is currently being handled
    // otherwise, the signal number
    int sig_current;


    // context before the signal handler was called
    // which is to be restored when the signal handler
    // returns (on receiving a SIGRETURN system call)
    // it resides in the kernel thread stack
    struct gp_regs* sig_return_context;


    // set of SHMs opened by the process and registered
    // using process_register_shm()
    struct shm_instance** shms;

    // number of registered SHM
    int n_shms;


    // unsorted array of futex waiters
    struct futex_waiter* futex_waiters;

    // number of futex waiters
    int n_futex_waiters;
} process_t;


// 0 on success
// the process is supposed to be locked,
// interrupts are disabled
int process_register_signal_setup(process_t* process, 
    void* user_handler,sigmask_t ign_mask,sigmask_t blk_mask);


// returns 0 on success
int process_trigger_signal(pid_t pid, int signal);



// if proc is NULL, then return whether any 
// process is mapped
int is_process_mapped(process_t* proc);



// register an shm object. This object will be
// released using process_remove_shm.
// When the process is freed, the remaining
// shm objects are also freed, before the 
// process memory mapping so that the memory
// can be correctly freed.
int process_register_shm(process_t*, struct shm_instance*);
void process_remove_shm(process_t*, struct shm_instance*);

// return the virtual base address of the the shm with
// given id. NULL is returned on failure:
// if the shm is not registered to the process.
void* process_get_shm_vbase(process_t*, shmid_t id);


// this function should be executed when a thread reaches the
// end of a signal handler: on receiving a SIGRETURN system call
// 
// returns non 0 value on failure
// return 0 on success
// the interrupts must be disabled, and
// stay disabled.
// 
// On success, this function replaces the thread context 
// so the caller must exit from the system call
// and yield so that the context is safely switched
int process_end_of_signal(process_t* process);


// call this function when a process has a non-0 
// sig_pending field, and handling a signal sis safe 
// (thread 0 is interruptible)
int process_handle_signal(process_t* process);


/**
 * @brief create a process with a given elf file
 * the elf file is not borrowed. The caller must
 * free it afterwards.
 * 
 * this functions initializes a process with:
 *  - no opened file
 *  - single thread executing the elf entry 
 * 
 * no user memory should be mapped before calling
 * this function, and no user memory is mapped
 * when it returns.
 * 
 * On failure, no process is allocated
 *  
 * 
 * @param process output process structure
 * @param pparent parent process structure
 * if pparent is NULL, the process will have
 * no parent (its parent's pid is the kernel pid)
 * @param elf_file input file
 * @return int 0 if failure, non-0 otherwise
 */
int create_process(process_t* process, process_t* pparent, 
            const void* elffile, size_t elffile_sz, fd_mask_t fd_mask);



/**
 * create a new thread for the given process
 * process should be locked, interrupts should be 
 * disabled
 * 
 */

tid_t process_create_thread(process_t* proc, void* entry, void* argument);

/**
 * @brief Create a kernel process with pid 0,
 * ppid 0, no program, 1 thread, no file
 * 
 * 
 * 
 */
int create_kernel_process(process_t* process);

/**
 * @brief Set the process entry arguments object
 * interrupts should be disabled before 
 * calling this function
 * argv and envp are null terminated strings with 
 * NULL last element
 * 
 */
int set_process_entry_arguments(process_t* process, 
        char* argv, size_t argv_sz, char* envp, size_t envp_sz);

/**
 * @brief free an exited process
 * (no remainng thread)
 * 
 * @param process 
 */
void free_process(process_t* process);


/**
 * In order to replace the process,
 * the current process must already
 * be mapped.
 * The process stays mapped
 */
int replace_process(process_t* process, void* elffile, size_t elffile_sz);


// map the process memory (private and shared)
// on the lower half. The process should be mapped
// already
void process_map(process_t* p);

// unmap the current process's memory
// (private and shared)
// a process should be mapped already
void process_unmap(void);


void process_futex_push_waiter(process_t* proc, void* uaddr, tid_t tid);

// this function releases the process lock during its execution.
// num < 0: broadcast futex
void process_futex_wake       (process_t* proc, void* uaddr, int num);
void process_futex_drop_waiter(process_t* proc, tid_t tid);




/**
 * closes the fd
 * replaces its type with FD_NONE
 * and calls the right close
 * function
 * 
 * @return int 0 if success, non-0 otherwise
 */
int close_fd(file_descriptor_t* fd);

/**
 * duplicates fd to new_fd
 * 
 * sets new_fd's fields
 * 
 */
void dup_fd(file_descriptor_t* fd, file_descriptor_t* new_fd);