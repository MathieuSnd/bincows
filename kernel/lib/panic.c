#include "panic.h"
#include "../lib/sprintf.h"
#include "../drivers/terminal/terminal.h"
#include "../drivers/ps2kb.h"
#include "../acpi/power.h"
#include "../int/apic.h"
#include "../memory/vmap.h"
#include "../memory/heap.h"
#include "../memory/paging.h"
#include "../sched/sched.h"
#include "../int/idt.h"
#include "stacktrace.h"


extern const size_t stack_size;
extern uint8_t stack_base[];


static int in_panic = 0;


__attribute__((noreturn)) void panic(const char* panic_string) {
    int level2 = in_panic;

    in_panic = 1;
    _cli();


    //driver_t* terminal = get_active_terminal();
//
    //if(terminal) // well, we cannot print colors
    //    terminal_set_colors(terminal, 0xfff0a0, 0x400000);

    if(panic_string == NULL)
        panic_string = "(null)";


    puts(
        "\x1b[41m"
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
        "!!!!!!!!!!!!!   KERNL PANIC   !!!!!!!!!!!!!\n"
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    
    puts(panic_string);
    puts("\n\n");

    if(level2) {
        puts("double panic: halting system");
        _cli();
        for(;;) asm volatile("hlt");
    }
    
    
    stacktrace_print();

clock_ns();
    printf("paniced at %u.%u ms\n", clock_ns() / 1000000, (clock_ns() / 1000) % 1000);
    printf("current pid: %d ; tid: %d\n", sched_current_pid(), sched_current_tid());

    if(sched_current_pid() != 0 && is_process_mapped(NULL) == 0) {
        // map user
        //_cli();
        process_t* pr = sched_get_process(sched_current_pid());

        process_map(pr);
        // set_user_page_map(pr->saved_page_dir_paddr);

        //_sti();
    }

    printf("heap:  %lx -> %lx (size: %6lx B, %5u KB, %u allocations)\n", 
            KERNEL_HEAP_BEGIN, 
            heap_get_brk(), 
            heap_get_size(),
            heap_get_size() / 1024,
            heap_get_n_allocation()
    );
    printf("stack: %lx -> %lx (size: %6lx B, %5u KB, %lx B free)\n", 
            stack_base, 
            &level2, 
            (void*)&level2 - (void*)stack_base, 
            ((void*)&level2 - (void*)stack_base) / 1024, 
            (void*)stack_base + stack_size - (void*)&level2
    );

    puts(
        "\n\n"
        "type ESCAPE to shutdown the computer.\n"
    );
    // do not make any interrupt
    ps2kb_poll_wait_for_key(PS2KB_ESCAPE);
    shutdown();

    asm volatile("hlt");
    
    __builtin_unreachable();
}