#include "../lib/assert.h"
#include "../memory/heap.h"
#include "../memory/vmap.h"
#include "../memory/paging.h"
#include "../lib/logging.h"

#include "sched.h"

#define FIRST_TID 1
#define STACK_SIZE 4096

// return stack base
// we suppose that the process is already mapped
// we find a stack address by polling user top memory 
static void* alloc_stack(process_t* process) {
    // @todo this doesnt work with more than 1 thread
    void* base = (void*)((USER_END + 1)/32) - STACK_SIZE;
    (void) process;

    alloc_pages(
        base, 
        STACK_SIZE >> 12, 
        PRESENT_ENTRY | PL_US | PL_RW
    );

    return base;
}


int create_process(process_t* process, process_t* pparent, const void* elffile, size_t elffile_sz) {

    elf_program_t* program = elf_load(elffile, elffile_sz);
    
    if(!program)
        return 0;

    pid_t ppid = KERNEL_PID;

    size_t n_files = 0;
    file_handle_t** files = NULL;

    if(pparent) {
        ppid = pparent->pid;

        // inherit parent file handlers
        n_files = pparent->n_files;
        files = malloc(sizeof(file_handle_t*) * n_files);

        for(unsigned i = 0; i < n_files; i++) {
         //   files[i] = vfs_clone_handle(pparent->files[i]);
        }
    }

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
        .files   = files,
        .n_files = n_files,
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

    assert(!process->n_threads);

    if(process->files) {
        for(unsigned i = 0; i < process->n_files; i++) {
            vfs_close_file(process->files[i]);
        }

        free(process->files);
    }


    if(process->threads)
        // thread buffer hasn't been freed yet
        free(process->threads);

    free(process);
}

