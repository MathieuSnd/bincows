#include "panic.h"
#include "../lib/sprintf.h"
#include "../drivers/terminal/terminal.h"
#include "../drivers/ps2kb.h"
#include "../acpi/power.h"
#include "../memory/vmap.h"
#include "../memory/heap.h"
#include "../sched/sched.h"
#include "stacktrace.h"


extern const size_t stack_size;
extern uint8_t stack_base[];


__attribute__((noreturn)) void panic(const char* panic_string) {

    driver_t* terminal = get_active_terminal();

    if(terminal) // well, we cannot print colors
        terminal_set_colors(terminal, 0xfff0a0, 0x400000);

    if(panic_string == NULL)
        panic_string = "(null)";

    puts(
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
        "!!!!!!!!!!!!!   KERNL PANIC   !!!!!!!!!!!!!\n"
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    
    puts(panic_string);
    puts("\n\n");
    
    
    stacktrace_print();


    printf("current pid: %d ; tid: %d\n", sched_current_pid(), sched_current_tid());

    printf("heap:  %lx -> %lx (size: %6lx B, %5u KB, %u allocations)\n", 
            KERNEL_HEAP_BEGIN, 
            heap_get_brk(), 
            heap_get_size(),
            heap_get_size() / 1024,
            heap_get_n_allocation()
    );
    printf("stack: %lx -> %lx (size: %6lx B, %5u KB, %lx B free)\n", 
            stack_base, 
            &terminal, 
            (void*)&terminal - (void*)stack_base, 
            ((void*)&terminal - (void*)stack_base) / 1024, 
            (void*)stack_base + stack_size - (void*)&terminal
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