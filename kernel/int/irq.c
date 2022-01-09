#include <stdint.h>
#include "../lib/assert.h"
#include "../lib/panic.h"

#include "irq.h"

// 0: 
static uint8_t irqs[256 - 32];


extern __attribute__((interrupt)) 
void IRQ_dummy_handler(struct IFrame* interrupt_frame);


unsigned install_irq(irq_handler_t* handler) {
    for(int i = 0; i < 256-32; i++) {
        if(!irqs[i]) {
            irqs[i] = 1;
            // the i-th idt entry isn't used
            set_irq_handler(i+32, handler);
            return i+32;
        }
    }
    panic("install_irq: no available IRQ");
    __builtin_unreachable();
}

void release_irq(unsigned n) {
    assert(n < 256);
    assert(n >= 32);
    
    // replace the handler by the dummy one
    set_irq_handler(n, IRQ_dummy_handler);

    // mark it as free
    irqs[n- 32] = 0;
}
