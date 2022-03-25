#include "../lib/assert.h"
#include "../memory/heap.h"
#include "../memory/vmap.h"
#include "../memory/paging.h"

#include "sched.h"

#define FIRST_TID 1
#define STACK_SIZE 256*1024

// return stack base
// we suppose that the process is already mapped
// we find a stack address by polling user top memory 
static void* alloc_stack(process_t* process) {
    // @todo this doesnt work with more than 1 thread
    void* base = (void*)USER_END + 1 - STACK_SIZE;
    (void) process;

    alloc_pages(
        base, 
        STACK_SIZE >> 12, 
        PRESENT_ENTRY | PL_XD
    );

    return base;
}


int create_process(process_t* process, const void* elffile, size_t elffile_sz) {

    elf_program_t* program = elf_load(elffile, elffile_sz);
    
    if(!program)
        return 0;

    thread_t* threads = malloc(sizeof(thread_t));


    pid_t pid = alloc_pid();

    assert(process);

    create_thread(
        &threads[0], 
        pid, 
        alloc_stack(process), 
        STACK_SIZE, 
        FIRST_TID
    );

    threads[0].regs.rip = program->entry;


    *process = (process_t) {
        .files   = NULL,
        .n_files = 0,
        .n_threads = 1,
        .threads = threads,
        .page_dir_paddr = 0,
        .pid = pid,
        .ppid = KERNEL_PID,
        .program = program,
    };

    return 1;
}


void free_process(process_t* process) {
    assert(process);

}

