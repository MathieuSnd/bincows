#include "power.h"
#include "../lib/assert.h"
#include "../lib/panic.h"
#include "../lib/logging.h"
#include "../drivers/ps2kb.h"
#include "../int/idt.h"
#include "../memory/heap.h"

static void (*funcs[MAX_SHUTDOWN_FUNCS])(void);
static int n_funcs = 0;
// register function to execute
// right before the computer
// shuts down / reboots
void atshutdown(void (*fun)(void)) {
    assert(n_funcs < MAX_SHUTDOWN_FUNCS);
    funcs[n_funcs++] = fun;
}

void shutdown(void) {
    // cannot shutdown anything yet.
    // need an AML interpreter.
    reboot();
}


// reboot the computer using the 
// ps/2 controller
void reboot(void) {
    for(int i = 0; i < n_funcs; i++)
        funcs[i]();



    unsigned still_allocated = heap_get_n_allocation();

    if(still_allocated) {
        log_warn("%d FREE BLOCKS AT SHUTDOWN", still_allocated);
        panic("oui");
    }
    //panic("non");
    //_cli();
    
    ps2_trigger_CPU_reset();

    for(;;)
        asm("hlt"); 
}
