#include "panic.h"
#include "../klib/sprintf.h"
#include "../video/terminal.h"


int zero = 0;

#define MAX_STACK_TRACE 15

extern uint64_t _rbp(void);

// always inline: make sure 
static __attribute__((always_inline)) void stack_trace(void) {
    void** ptr = (void**)_rbp();
    kputs("backtrace:\n");
    
    for(unsigned i = 0; i < MAX_STACK_TRACE; i++) {
        
        if(*ptr == 0) // reached the top
            break;
        kprintf("\t%llx \n", *(ptr+1));

        ptr = *ptr;
    }
}

__attribute__((noreturn)) void panic(const char* panic_string) {
    // checks if video is operationnal
    if(get_terminal_handler() != NULL) {
        terminal_set_colors(0xfff0a0, 0x400000);

        if(panic_string == NULL)
            panic_string = "(null)";

        kputs(
            "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
            "!!!!!!!!!!!!!   KERNL PANIC   !!!!!!!!!!!!!\n"
            "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        
        kputs(panic_string);
        kputs("\n\n");
        
        stack_trace();

        kputs(
            "\n\n"
            "you may manually shutdown the machine.\n"
        );
    }
    
    asm volatile("cli");
    asm volatile("hlt");
    
    __builtin_unreachable();
}