#include "panic.h"
#include "../klib/sprintf.h"
#include "../video/terminal.h"


__attribute__((noreturn)) void panic(const char* panic_string) {
    // checks if video is operationnal
    if(get_terminal_handler() != NULL) {
        terminal_set_colors(0xa0a0a0, 0x0000ff);

        if(panic_string == NULL)
            panic_string = "(null)";

        kprintf(
            "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
            "!!!!!!!!!!!!!   KERNL PANIC   !!!!!!!!!!!!!\n"
            "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
            "%s\n\n"
            "you may manually shutdown the machine.\n",
            panic_string
        );
    }
    for(;;)
        asm volatile("hlt");
    __builtin_unreachable();
}