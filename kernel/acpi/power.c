#include "power.h"
#include "../lib/assert.h"
#include "../lib/sprintf.h"
#include "../lib/panic.h"
#include "../lib/logging.h"
#include "../drivers/ps2.h"
#include "../int/idt.h"
#include "../memory/heap.h"
#include <lai/helpers/pm.h>

static void (*funcs[MAX_SHUTDOWN_FUNCS])(void);
static int n_funcs = 0;
// register function to execute
// right before the computer
// shuts down / reboots
void atshutdown(void (*fun)(void)) {
    assert(n_funcs < MAX_SHUTDOWN_FUNCS);
    funcs[n_funcs++] = fun;
}

static void exit(void) {
    for(int i = 0; i < n_funcs; i++)
        funcs[i]();

    n_funcs = 0;


    unsigned still_allocated = heap_get_n_allocation();

    if(still_allocated) {
#if !defined(NDEBUG) && !defined(USE_LAI)
        heap_defragment();
        heap_print();
        log_warn("%d ALLOCATED BLOCKS AT SHUTDOWN", still_allocated);
        panic("memory leaks");
#endif
    }
}



void shutdown(void) {
    exit();
    // insert a newline as we don't know what's 
    // being currently printed
    printf("\n");
    log_debug("shutdown sequence started");

#ifdef USE_LAI
    log_debug("ACPI shutdown...");
    int error = lai_enter_sleep(5);
    asm volatile("hlt");
    log_warn("LAI error %u", error);
    panic("ACPI shutdown failed");
#else
    log_warn("cannot make ACPI shutdown, rebooting instead");
    reboot();
#endif
}


// reboot the computer using the 
// ps/2 controller
void reboot(void) {
    exit();

#ifdef USE_LAI
    log_debug("ACPI reboot...");

    int error = lai_acpi_reset();

    log_warn("ACPI reboot failed, error %u. Trying ps2 reset instead", error);
#else
    log_warn("ps2 CPU reset...");
#endif

    // in case it didn't work

    ps2_trigger_CPU_reset();

    for(;;)
        asm("hlt"); 
}
