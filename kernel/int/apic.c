#include <stdint.h>

#include "../debug/assert.h"
#include "idt.h"
#include "apic.h"
#include "../klib/sprintf.h"

volatile struct APICConfig* apic_config = NULL;


static inline void outb(uint16_t port, uint8_t val)
{
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}
uint8_t inb(uint16_t dx);

void     set_rflags(uint64_t rf);
uint64_t get_rflags(void);


static unsigned read_pit_count(void) {
	unsigned count = 0;
 
	// Disable interrupts
    uint64_t rf = get_rflags();
	_cli();

 
	count = inb(0x40);		// Low byte
	count |= inb(0x40)<<8;		// High byte
    

    set_rflags(rf);

	return count;
}


#define ABS(X) (X<0 ? -X : X)


static void pit_init() {
	// al = channel in bits 6 and 7, remaining bits clear
	outb(0x43,0b0000000);
    outb(0x40, 0xff);
    outb(0x40, 0xff);
} 


static void pit_wait(size_t ms) {    

    uint32_t delta = ms * 0x04a9;

// no overflow
    //assert(delta > 0xffff == 0);

    uint32_t cur = read_pit_count();

    uint16_t end = (cur + delta) % 0x10000;
// 16 bit counter
    uint16_t dif = delta, ndif;
    while(1) {
        uint16_t pit = read_pit_count();
        
        uint16_t ndif = ABS(end - pit);

        if(ndif > dif)
            break;
        dif = ndif;
    }
        //asm volatile("pause");
    return;



    /*
    uint16_t val = 0x4a9 * ms;     
    outb(0x40, (uint8_t)val); 
    outb(0x40, (uint8_t)(val >> 8));     
    outb(0x43, 0x30);     
    for(;;) {         
        outb(0x43, 0xe2);         
        uint8_t status = inb(0x40);         
        if ((status & (1 << 7)) != 0) {             
            break;
        }
    }
    */
}



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
    apic_config->spurious_interrupt_vector.reg = 1 | LAPIC_SPURIOUS_IRQ; 
    
    // masks the irq, one shot mode
    apic_config->LVT_timer.reg = 0x10000; 

    apic_config->timer_divide_configuration.reg = 3; // divide by 16
    apic_config->timer_initial_count.reg = UINT32_MAX;

    for(int i = 0; i < 10; i++)
        pit_wait(1); /// wait for 1 ms 

    uint32_t t = (UINT32_MAX - apic_config->timer_current_count.reg);

    apic_config->timer_initial_count.reg = t;
            // irq on 

    // enable apic and set spurious int to 0xff
    apic_config->spurious_interrupt_vector.reg = 0x100 | LAPIC_SPURIOUS_IRQ; 

// unmask the IRQ, periodic mode, timer on irq 32
    apic_config->LVT_timer.reg = 0x20020;

}

