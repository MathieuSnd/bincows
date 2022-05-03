#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../lib/elf/elf.h"

#include "../fs/vfs.h"

// this file describes what are processes

typedef int pid_t;

typedef unsigned fd_t;

#define MAX_FDS 32

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

} process_t;


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
int create_process(process_t* process, process_t* pparent, const void* elffile, size_t elffile_sz);


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