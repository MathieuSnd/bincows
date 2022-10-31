#include <stdint.h>
#include "../lib/assert.h"
#include "../lib/panic.h"
#include "../lib/logging.h"
#include "../lib/sprintf.h"
#include "../sched/sched.h"

#include "irq.h"


typedef struct {
    struct driver* driver;
    void* handler;
} irq_data_t;

// handler == NULL:     no irq is mapped
// ints[0] = int handler for irq 0
// int[IRQ_BEGIN] = int handler for first irq
static irq_data_t ints[IRQ_END + 1] = {0};


extern __attribute__((interrupt)) 
void IRQ_dummy_handler(struct IFrame* interrupt_frame);



void register_irq(
    unsigned       irq_number,
    irq_handler_t  handler, 
    struct driver* driver
) {
    assert(irq_number <= IRQ_END);
    assert(irq_number >= IRQ_BEGIN);

    // assert that no handler is already 
    // installed
    assert(!ints[irq_number].handler);

    ints[irq_number].handler = handler;
    ints[irq_number].driver  = driver;

}

unsigned register_exception_handler(
    unsigned exception_number,
    exc_handler_t handler
) {
    // exceptions are 0 -> 31,
    // IRQs come right after
    assert(exception_number < IRQ_BEGIN);
    assert(handler);
    // there can only be one exception handler.
    // if there is already one, it is probably 
    // an error.
    assert(!ints[exception_number].handler);


    ints[exception_number].handler = handler;
    return exception_number;
}

unsigned install_irq(irq_handler_t  handler,
                     struct driver* driver) {
    for(int i = IRQ_BEGIN; i < IRQ_END + 1; i++) {
        if(!ints[i].handler) {
            ints[i].handler = handler;
            ints[i].driver  = driver;
            // the i-th idt entry isn't used
            return i;
        }
    }
    panic("install_irq: no available IRQ");
    __builtin_unreachable();
}

void release_irq(unsigned n) {
    assert(n <= IRQ_END);
    assert(n >= IRQ_BEGIN);

    // assert that a handler is installed
    assert(ints[n].handler);
    
    // mark it as free
    ints[n].handler = NULL;
}


// called from irq.s
// if the handler is called from an exception with error code,
// the error code is passed as the third argument.
// Otherwise the error_code field is reserved.
void int_common_handler(uint8_t irq_n, gp_regs_t* context, uint32_t error_code) {
    assert(irq_n <= IRQ_END);


    // the irq_n th irq just fired
    void*         driver  = ints[irq_n].driver;
    irq_handler_t handler = ints[irq_n].handler;

    if(!handler) {
        char buff[64];
        sprintf(buff, "IRQ%u fired, but no handler is specified", irq_n);
        panic  (buff);
    }


    sched_save(context);

    if(sched_is_running()) {

        // debug thing
        process_t* p = sched_current_process();

        assert(p->pid == sched_current_pid());


        
        // single thread
        //assert(p->threads[0].tid == sched_current_tid());
        //assert(p->threads[0].pid == sched_current_pid());
        //assert(p->threads[0].state == READY);

        spinlock_release(&p->lock);
    }

    if(irq_n < IRQ_BEGIN) {

        log_warn("EXCEPTION %u fired", irq_n);
        // exception
        ((exc_handler_t)(void*)handler)(error_code);
    } else {
        // irq
        ((irq_handler_t)handler)(driver);
        sched_irq_ack();
    }

}

