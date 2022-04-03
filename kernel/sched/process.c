#include "../lib/assert.h"
#include "../memory/heap.h"
#include "../memory/vmap.h"
#include "../memory/paging.h"
#include "../lib/logging.h"
#include "../lib/string.h"

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

    // assert that no userspace is mapped
    assert(get_user_page_map() == 0);

    uint64_t user_page_map = alloc_user_page_map();

    set_user_page_map(user_page_map);

    // we have to have the userspace map ready
    // before calling this
    elf_program_t* program = elf_load(elffile, elffile_sz);
    
    if(!program)
        return 0;

    pid_t ppid = KERNEL_PID;

    file_handle_t** files = NULL;

    files = malloc(sizeof(file_handle_t*) * MAX_FDS);

    if(pparent) {
        // inherit parent file handlers
        ppid = pparent->pid;


        for(unsigned i = 0; i < MAX_FDS; i++) {
            if(pparent->files[i])
                files[i] = vfs_clone_handle(pparent->files[i]);
        }
    }
    else
        memset(files, 0, sizeof(file_handle_t*) * MAX_FDS);


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

    threads[0].rsp->rip = (uint64_t)program->entry;


    // choose an emplacement for heap base
    // choose it above the highest elf 
    // segment's end
    void* elf_end = NULL;
    for(unsigned i = 0; i < program->n_segs; i++) {
        void* segment_end = program->segs[i].base + program->segs[i].length;
     
        if((uint64_t)elf_end < (uint64_t)segment_end)
            elf_end = segment_end;
    }

    // empty heap
    process->brk = process->unaligned_brk = process->heap_begin = 
                        (void *)(((uint64_t)elf_end+0xfff) & ~0x0fffllu);
    


    *process = (process_t) {
        .files   = files,
        .n_threads = 1,
        .threads = threads,
        .page_dir_paddr = user_page_map,
        .pid = pid,
        .ppid = ppid,
        .program = program,
    };
    

    return 1;
}


void free_process(process_t* process) {
    assert(process);

    assert(!process->n_threads);

    for(unsigned i = 0; i < MAX_FDS; i++) {
        if(process->files)
            vfs_close_file(process->files[i]);
    }

    free(process->files);


    if(process->threads)
        // thread buffer hasn't been freed yet
        free(process->threads);

    elf_free(process->program);

    free(process);
}

