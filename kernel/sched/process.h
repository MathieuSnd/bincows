#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../lib/elf/elf.h"

#include "../fs/vfs.h"

// this file describes what are processes

typedef int pid_t;

typedef unsigned fd_t;

#define MAX_FDS 32



typedef struct process {
    pid_t pid;

    pid_t ppid;

    uint64_t page_dir_paddr;

    size_t n_threads;
    struct thread* threads;


    elf_program_t* program;


    void* heap_begin;
    void* brk;

    // brk as seen by processes
    void* unaligned_brk;

    
    file_handle_t* files[MAX_FDS];

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
 * @brief free an exited process
 * (no remainng thread)
 * 
 * @param process 
 */
void free_process(process_t* process);



