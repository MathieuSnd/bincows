#include "panic.h"
#include "../lib/sprintf.h"
#include "../drivers/terminal/terminal.h"
#include "../drivers/ps2kb.h"
#include "../acpi/power.h"
#include "../memory/vmap.h"


int zero = 0;

#define MAX_STACK_TRACE 15

extern uint64_t _rbp(void);

// always inline: make sure 
static inline __attribute__((always_inline)) void stack_trace(void) {
    void** ptr = (void**)_rbp();
    puts("backtrace:\n");
    
    for(unsigned i = 0; i < MAX_STACK_TRACE; i++) {
        //printf("oui. %lx ", ptr);
        
        if(*ptr == 0) // reached the top
            break;
        
        void* rip = *(ptr+1);
        if(!is_kernel_memory((uint64_t)rip)) {
            //maybe it is an exception error code
            rip = *(ptr+2);
            
            printf("   %llx - EXCEPTION\n", rip);
        }
        else
            printf("   %llx\n", rip);


        ptr = *ptr;
    }
}

__attribute__((noreturn)) void panic(const char* panic_string) {

    // checks if video is operationnal
    if(get_terminal_handler() != NULL) {
        terminal_set_colors(0xfff0a0, 0x400000);

        if(panic_string == NULL)
            panic_string = "(null)";

        puts(
            "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
            "!!!!!!!!!!!!!   KERNL PANIC   !!!!!!!!!!!!!\n"
            "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        
        puts(panic_string);
        puts("\n\n");
        
        stack_trace();

        puts(
            "\n\n"
            "type ESCAPE to shutdown the computer.\n"
        );
        // do not make any interrupt
        ps2kb_poll_wait_for_key(PS2KB_ESCAPE);
        shutdown();
    }
    asm volatile("cli");
    asm volatile("hlt");
    
    __builtin_unreachable();
}