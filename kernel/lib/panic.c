#include "panic.h"
#include "../lib/sprintf.h"
#include "../drivers/terminal/terminal.h"
#include "../drivers/ps2kb.h"
#include "../acpi/power.h"
#include "../memory/vmap.h"
#include "stacktrace.h"


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

    puts(
        "\n\n"
        "type ESCAPE to shutdown the computer.\n"
    );
    // do not make any interrupt
    ps2kb_poll_wait_for_key(PS2KB_ESCAPE);
    shutdown();

    asm volatile("cli");
    asm volatile("hlt");
    
    __builtin_unreachable();
}