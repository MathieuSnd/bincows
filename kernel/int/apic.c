#include <stdint.h>

#include "../debug/assert.h"
#include "idt.h"
#include "apic.h"
#include "../klib/sprintf.h"

#define LAPIC_TIMER_IRQ 0xff

struct APICConfig* apic_config = NULL;


void outb(uint16_t dx, uint16_t al);
uint8_t inb(uint16_t dx);


static void pit_wait(size_t ms) {
    outb(0x43, 0x30);     
    uint16_t val = 0x4a9 * ms;     
    outb(0x40, (uint8_t)val); 
    outb(0x40, (uint8_t)(val >> 8));     
    for(;;) {         
        outb(0x43, 0xe2);         
        uint8_t status = inb(0x40);         
        if ((status & (1 << 7)) != 0) {             
            break;
        }     
    } 
}

static unsigned read_pit_count(void) {
	unsigned count = 0;
 
	// Disable interrupts
    uint64_t rf = get_rflags();
	_cli();
 
	// al = channel in bits 6 and 7, remaining bits clear
	outb(0x43,0b0000000);
 
	count = inb(0x40);		// Low byte
	count |= inb(0x40)<<8;		// High byte
    

    set_rflags(rf);

	return count;
}


uint64_t apic_timer_clock_count = 0;


__attribute__((interrupt)) void lapic_timer_handler(struct IFrame* frame) {
    (void) frame;
    ++apic_timer_clock_count;

    kprintf("%x\n", apic_timer_clock_count);
    
    apic_config->end_of_interrupt.reg = 0;
}  


uint64_t clock(void)  {
    return apic_timer_clock_count;
}

void apic_setup_clock(void) {
    assert(apic_config != NULL);

    set_irq_handler(LAPIC_TIMER_IRQ, lapic_timer_handler);

    // disable apic and set spurious int to 32
    apic_config->spurious_interrupt_vector.reg = 0 | LAPIC_TIMER_IRQ; 

    apic_config->timer_divide_configuration.reg = 2; // divide by 8
    apic_config->timer_initial_count.reg = UINT32_MAX;

    pit_wait(1); /// wait for 1 ms 

    //apic_config->timer_initial_count.reg = UINT32_MAX - apic_config->timer_current_count.reg;


    for(int i = 0; i < 1000000000; i++) {
        kprintf("%x\t%x\n", apic_config->timer_current_count.reg, read_pit_count());
       
       // asm volatile("hlt");
    }
    // count 


    // enable the clock irqs
    apic_config->spurious_interrupt_vector.reg = 0x100 | LAPIC_TIMER_IRQ; 
}

