#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../lib/elf/elf.h"

#include "../fs/vfs.h"

#include "signal/signal.h"

// this file describes what are processes

typedef int pid_t;
typedef int32_t tid_t;


typedef unsigned fd_t;

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




typedef struct process {
    pid_t pid;

    pid_t ppid;

    uint64_t page_dir_paddr;

    // might be different from
    // page_dir_paddr if the 
    // kernel was doing stuf
    // with another process
    // when it got interrupted
    uint64_t saved_page_dir_paddr;

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

    /**
     * this table contains the signal handlers
     * for the process. It resides in userspace
     * and is registered to the kernel using
     * the REGISTER_SIGNALS system call.
     * 
     */
    sighandler_t* sig_table;

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

    // address of the beginning of the return function
    // this function should be a non return function
    // that simply invokes a SIGRETURN system call
    void* sig_return_function;

    // context before the signal handler was called
    // which is to be restored when the signal handler
    // returns (on receiving a SIGRETURN system call)
    // it resides in the kernel thread stack
    struct gp_regs* sig_return_context;

    // address of the return context


} process_t;


// 0 on success
// the process is supposed to be locked,
// interrupts are disabled
int process_register_signal_setup(
            process_t* process, void* function, sighandler_t* table);


// returns 0 on success
int process_trigger_signal(pid_t pid, int signal);


// returns 0 on success
// should be executed when a thread reaches the
// end of a signal handler: on receiving a SIGRETURN system call
int process_end_of_signal(process_t* process);


/**
 * @brief create a process with a given elf file
 * the elf file is not borrowed. The caller must
 * free it afterwards.
 * 
 * this functions initializes a process with:
 *  - no opened file
 *  - single thread executing the elf entry 
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