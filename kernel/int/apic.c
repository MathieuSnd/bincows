#include <stdint.h>

#include "../debug/assert.h"
#include "idt.h"
#include "apic.h"
#include "../klib/sprintf.h"
#include "../memory/vmap.h"
#include "../drivers/hpet.h"

volatile struct APICConfig* apic_config = APIC_VIRTUAL_ADDRESS;

void     set_rflags(uint64_t rf);
uint64_t get_rflags(void);


uint64_t apic_timer_clock_count = 0;


char buffer[128] = {0};

__attribute__((interrupt)) void lapic_timer_handler(struct IFrame* frame) {
    (void) frame;
    ++apic_timer_clock_count;

    apic_config->end_of_interrupt.reg = 0;
}  

uint64_t clock(void)  {
    return apic_timer_clock_count;
}


extern uint64_t read_msr(uint32_t address);
extern void     write_msr(uint32_t address, uint64_t value);



inline uint64_t read(uint32_t m_address) {
    uint32_t low, high;

    asm("rdmsr" : "=a"(low), "=d"(high) : "c"(m_address));

    return ((uint64_t) high << 32) | low;
}


#define PRINT(X) kprintf("%s = %lx\n", #X, X)

void apic_setup_clock(void) {

    
// enable apic msr
    uint64_t IA32_APIC_BASE = read_msr(0x1b);    
    IA32_APIC_BASE |= (1 << 11);
    write_msr(0x1b, IA32_APIC_BASE);


    assert(apic_config != NULL);
    set_irq_handler(32, lapic_timer_handler);

    pit_init();



    // enable apic and set spurious int to 0xff
    apic_config->spurious_interrupt_vector.reg = 0x100 | LAPIC_SPURIOUS_IRQ; 
    
    // masks the irq, one shot mode
    apic_config->LVT_timer.reg = 0x20000; 

    apic_config->timer_divide_configuration.reg = 3; // divide by 16

    hpet_prepare_wait_ms(1);

    apic_config->timer_initial_count.reg = UINT32_MAX;

    hpet_wait();

//    for(int i = 0; i < 10; i++) 
//        pit_wait(1); /// wait for 1 ms 

    //terminal_clear();

    uint32_t t = (UINT32_MAX - apic_config->timer_current_count.reg);

    apic_config->timer_initial_count.reg = t;
            // irq on 

    // enable apic and set spurious int to 0xff
    
// unmask the IRQ, periodic mode, timer on irq 32
    apic_config->LVT_timer.reg = 0x20020;

}

