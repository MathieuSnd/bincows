#include "../lib/assert.h"
#include "../memory/heap.h"
#include "../memory/vmap.h"
#include "../memory/paging.h"
#include "../lib/logging.h"
#include "../lib/string.h"
#include "../int/apic.h"

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


void dup_fd(file_descriptor_t* fd, file_descriptor_t* new_fd) {
    new_fd->type = fd->type;

    switch(fd->type) {
        case FD_FILE:
            new_fd->file = vfs_handle_dup(fd->file);
            break;
        case FD_DIR:
            new_fd->dir = vfs_dir_dup(fd->dir);
            new_fd->dir_boff = fd->dir_boff;
            break;
        case FD_NONE:
            break;
        default:
            assert(0);
    }
}


int close_fd(file_descriptor_t* fd) {
    if(fd->file == NULL) {
        return -1;
    }

    switch(fd->type) {
        case FD_FILE:
            vfs_close_file(fd->file);
            break;
        case FD_DIR:
            vfs_closedir(fd->dir);
            break;
        default:
            assert(0);
    }



    fd->type = FD_NONE;

    return 0;
}



int create_process(
        process_t*  process, 
        process_t*  pparent, 
        const void* elffile, 
        size_t      elffile_sz
) {

    // assert that no userspace is mapped
    assert(get_user_page_map() == 0);

    uint64_t user_page_map = alloc_user_page_map();

    set_user_page_map(user_page_map);

    // we have to have the userspace map ready
    // before calling this
    elf_program_t* program = elf_load(elffile, elffile_sz);

    
    if(!program) {
        log_debug("create_process: elf_load failed");
        return 0;
    }

    pid_t ppid = KERNEL_PID;

    file_descriptor_t* fds = NULL;

    fds = malloc(sizeof(file_descriptor_t) * MAX_FDS);

    char* cwd;

    // inherit parent file handlers
    ppid = pparent->pid;

    for(unsigned i = 0; i < MAX_FDS; i++)
        dup_fd(pparent->fds + i, fds + i);


    // inherit parent directory
    cwd = strdup(pparent->cwd);



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

    // set PC
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

    *process = (process_t) {
        .fds   = fds,
        .n_threads = 1,
        .threads = threads,
        .page_dir_paddr = user_page_map,
        .saved_page_dir_paddr = user_page_map,
        .pid = pid,
        .ppid = ppid,
        .cwd  = cwd,
        .program = program,
        .clock_begin = clock_ns()
    };
    

    // empty heap
    process->brk = process->unaligned_brk = process->heap_begin = 
                        (void *)(((uint64_t)elf_end+0xfff) & ~0x0fffllu);
    
    return 1;
}


extern void kernel_process_entry(void);

int create_kernel_process(process_t* process) {

    // create a process with a single thread
    thread_t* threads = malloc(sizeof(thread_t));
    *threads = (thread_t) {
        .exit_hooks = NULL,
        .n_exit_hooks = 0,
        .lock = 0,
        .pid = 0,
        .tid = 1,
        .state = BLOCKED,
        .running_cpu_id = 0,
        .should_exit = 0,
        .stack = (stack_t) {
            .base = NULL,
            .size = 0,
        },
        .kernel_stack = (stack_t) {
        .base = malloc(THREAD_KERNEL_STACK_SIZE),
        .size = THREAD_KERNEL_STACK_SIZE,
        },
    };

    void* stack_top = threads->kernel_stack.base 
                      + threads->kernel_stack.size;

    uint64_t* saved_frame = (uint64_t*)stack_top - 1;

    // frame top
    *saved_frame = (uint64_t)0;


    threads->rsp = stack_top - sizeof(gp_regs_t) - 8;

    assert(process);

    // set initial PC
    threads[0].rsp->rip = (uint64_t)kernel_process_entry;

    threads->rsp->cs  = KERNEL_CS;
    threads->rsp->ss  = KERNEL_DS;
    threads->rsp->rsp = (uint64_t)threads->rsp;

    threads->rsp->rflags = USER_RF; // doesn't really matter

    *process = (process_t) {
        .n_threads = 1,
        .threads = threads,
        .page_dir_paddr = 0,
        .saved_page_dir_paddr = 0,
        .pid  = 0,
        .ppid = 0,
        .cwd  = strdup("/"), // root directory
        .program = NULL,
        .clock_begin = clock_ns()
    };




    process->fds = malloc(sizeof(file_descriptor_t) * MAX_FDS);

    for(unsigned i = 0; i < MAX_FDS; i++) {
        process->fds[i].type = FD_NONE;
        process->fds[i].file = NULL;
    }



    log_info("kernel process created");

    return 1;
}



void free_process(process_t* process) {

    assert(interrupt_enable());
    assert(process);
    assert(!process->n_threads);

    if(process->pid == KERNEL_PID) {
        log_info("free_process: kernel process");
        // kernel process
    }

    for(unsigned i = 0; i < MAX_FDS; i++) {
        if(process->fds[i].type != FD_NONE) {
            close_fd(&process->fds[i]);
        }
    }

    free(process->fds);
    free(process->cwd);


    if(process->threads)
        // thread buffer hasn't been freed yet
        free(process->threads);

    if(process->program)
        elf_free(process->program);


    free(process);
}



int replace_process(process_t* process, void* elffile, size_t elffile_sz) {
    // assert that the process is already mapped
    assert(get_user_page_map() == process->page_dir_paddr);

    unmap_user();
    free_user_page_map(process->page_dir_paddr);

    // recreate page directory
    process->saved_page_dir_paddr =
    process->page_dir_paddr = alloc_user_page_map();

    set_user_page_map(process->page_dir_paddr);


    // only one thread
    process->threads = realloc(process->threads, sizeof(thread_t));

    int r = create_thread(
        &process->threads[0], 
        process->pid, 
        alloc_stack(process), 
        STACK_SIZE, 
        FIRST_TID
    );

    assert(!r);

    // load program
    elf_program_t* program = elf_load(elffile, elffile_sz);


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

    

    process->threads[0].rsp->rip = (uint64_t)program->entry;



    return 0;
}


static size_t string_list_len(char* list) {
    size_t len = 0;

    while(*list) {
        list += strlen(list) + 1;
        len++;
    }

    return len;
}


int set_process_entry_arguments(process_t* process, 
    char* argv, size_t argv_sz, char* envp, size_t envp_sz
) {

    assert(process);
    //setup stack

    uint64_t rsp = (uint64_t)process->threads[0].stack.base
                 + process->threads[0].stack.size;


    // align sizes to 16 bytes
    // because we are copying it in
    // a stack
    argv_sz = (argv_sz + 15) & ~0x0f;
    envp_sz = (envp_sz + 15) & ~0x0f;

    // put the arguments and env vars in the stack
    uint64_t user_argv = (rsp -= argv_sz);
    uint64_t user_envp = (rsp -= envp_sz);

    // frame begin
    uint64_t* user_frame_begin = (uint64_t*)(rsp -= sizeof(uint64_t));

    *user_frame_begin = 0;


    process->threads[0].rsp = (void*)(rsp -= sizeof(gp_regs_t));



    // stack.base < rsp < user_envp < user_argv
    if((void*)process->threads[0].rsp <= process->threads[0].stack.base) {
        // not enough space
        return -1;
    }

    // copy argv and envp
    memcpy((void*)user_argv, argv, argv_sz);
    memcpy((void*)user_envp, envp, envp_sz);

    // compute argc and envc
    int argc;
    int envc;

    if(argv)
        argc = string_list_len(argv);
    else 
        argc = 0;

    if(envp)
        envc = string_list_len(envp);
    else 
        envc = 0;

    // fill registers initial values

    // entry arguments:
    // _start(int argc, char* args, int envrionc, char* environ)
    process->threads[0].rsp->rdi = argc;
    process->threads[0].rsp->rsi = user_argv;
    process->threads[0].rsp->rdx = envc;
    process->threads[0].rsp->rcx = user_envp;


    process->threads[0].rsp->rsp = rsp;
    process->threads[0].rsp->rbp = (uint64_t)user_frame_begin;
    process->threads[0].rsp->rip = (uint64_t)process->program->entry;
    process->threads[0].rsp->cs  = USER_CS;
    process->threads[0].rsp->ss  = USER_DS;
    process->threads[0].rsp->rflags = USER_RF;


    for(int i = 0; i < 8; i++)
        process->threads[0].rsp->ext[i] = 0;
    
    // zero the other registers
    process->threads[0].rsp->rax = 0;
    process->threads[0].rsp->rbx = 0;
    
    return 0;
}

