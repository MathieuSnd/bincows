#include <stdint.h>
#include "../lib/assert.h"
#include "../lib/panic.h"
#include "../lib/logging.h"
#include "../lib/sprintf.h"
#include "../sched/thread.h"

#include "irq.h"



typedef struct {
    struct driver* driver;
    irq_handler_t handler;
} irq_data_t;

// handler == NULL:     no irq is mapped
static irq_data_t irqs[IRQ_END - IRQ_BEGIN + 1] = {0};


extern __attribute__((interrupt)) 
void IRQ_dummy_handler(struct IFrame* interrupt_frame);



void register_irq(
    unsigned       irq_number,
    irq_handler_t  handler, 
    struct driver* driver
) {
    assert(irq_number <= IRQ_END);
    assert(irq_number >= IRQ_BEGIN);

    irq_number -= IRQ_BEGIN;
    // assert that no handler is already 
    // installed
    assert(!irqs[irq_number].handler);

    irqs[irq_number].handler = handler;
    irqs[irq_number].driver  = driver;
}

unsigned install_irq(irq_handler_t  handler,
                     struct driver* driver) {
    for(int i = 0; i < IRQ_END - IRQ_BEGIN + 1; i++) {
        if(!irqs[i].handler) {
            irqs[i].handler = handler;
            irqs[i].driver  = driver;
            // the i-th idt entry isn't used
            return i+IRQ_BEGIN;
        }
    }
    panic("install_irq: no available IRQ");
    __builtin_unreachable();
}

void release_irq(unsigned n) {
    assert(n <= IRQ_END);
    assert(n >= IRQ_BEGIN);
    
    // replace the handler by the dummy one
    set_irq_handler(n, IRQ_dummy_handler);

    // mark it as free
    irqs[n - IRQ_BEGIN].handler = NULL;
}

// called from irq.s
void irq_common_handler(uint8_t irq_n, gp_regs_t* context) {
    assert(irq_n <= IRQ_END);
    assert(irq_n >= IRQ_BEGIN);
    // the irq_n th irq just fired
    void*         driver  = irqs[irq_n - IRQ_BEGIN].driver;
    irq_handler_t handler = irqs[irq_n - IRQ_BEGIN].handler;

    if(!handler) {
        char buff[64];
        sprintf(buff, "IRQ%u fired, but no handler is specified", irq_n);
        panic  (buff);
    }


    sched_save(context);

    handler(driver);

    if(!schedule())
        return;
}

