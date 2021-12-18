#include <stdint.h>

#include "idt.h"
#include "apic.h"

#include "../lib/assert.h"
#include "../lib/logging.h"
#include "../lib/sprintf.h"
#include "../memory/vmap.h"
#include "../drivers/hpet.h"
#include "../lib/string.h"
#include "../memory/heap.h"

struct APICRegister 
{
    volatile uint32_t reg;
    uint32_t reserved[3];
};
static_assert(sizeof(struct APICRegister) == 16);

struct APICConfig
{
    volatile struct APICRegister reserved1[2];                         // 
    volatile struct APICRegister LAPIC_ID;                             // RW
    volatile struct APICRegister LAPIC_version;                        // R
    volatile struct APICRegister reserved2[4];                         // 
    volatile struct APICRegister task_priority;                        // RW
    volatile struct APICRegister arbitration_priority;                 // R
    volatile struct APICRegister processor_priority;                   // R
    volatile struct APICRegister end_of_interrupt;                     // W
    volatile struct APICRegister remote_read;                          // R
    volatile struct APICRegister logical_destination;                  // RW
    volatile struct APICRegister destination_format;                   // RW
    volatile struct APICRegister spurious_interrupt_vector;            // RW
    volatile struct APICRegister in_service[8];                        // R
    volatile struct APICRegister trigger_mode[8];                      // R
    volatile struct APICRegister interrupt_request[8];                 // R
    volatile struct APICRegister error_status;                         // R
    volatile struct APICRegister reserved3[6];                         // 
    volatile struct APICRegister LVT_corrected_machine_check_interrupt;// RW
    volatile struct APICRegister interrupt_command[2];                 // RW
    volatile struct APICRegister LVT_timer;                            // RW
    volatile struct APICRegister LVT_thermal_sensor;                   // RW
    volatile struct APICRegister LVT_performance_monitoring_counters;  // RW
    volatile struct APICRegister LVT_LINT0;                            // RW
    volatile struct APICRegister LVT_LINT1;                            // RW
    volatile struct APICRegister LVT_error;                            // RW
    volatile struct APICRegister timer_initial_count;                  // RW
    volatile struct APICRegister timer_current_count;                  // R
    volatile struct APICRegister reserved4[4];                         // 
    volatile struct APICRegister timer_divide_configuration;           // RW
    volatile struct APICRegister reserved5;                            // 
};
static_assert(sizeof(struct APICConfig) == 0x400);


static volatile struct APICConfig* apic_config = (void *)APIC_VIRTUAL_ADDRESS;


static uint64_t apic_timer_clock_count = 0;


// 1 ms
#define CLOCK_FREQUENCY 1
typedef struct {
    unsigned period;    
    unsigned counter;
    char exists;
    
    void (*func)(void*);
    void*    param;
} timer_t;


// realloc(NULL, s) ~ malloc(s)
timer_t* timers = NULL;
static unsigned n_timers = 0;


static void timers_realloc(void) {
    static unsigned buffsize = 0;
    
    if(n_timers == 0) {
        // performs a free
        buffsize = 0;
    }
    
    if(n_timers > buffsize)
        buffsize *= 2;
    
    else if(n_timers < buffsize / 2)
        buffsize /= 2;
    
    timers = realloc(timers, buffsize*sizeof(timer_t));
}


unsigned apic_create_timer(timer_callback_t fun, int millisecs, void* param) {
    unsigned id = n_timers++;

    timers_realloc();

    timers[id].counter = 0;
    timers[id].period  = millisecs * CLOCK_FREQUENCY;
    timers[id].func    = fun;
    timers[id].func    = param;

    timers[id].exists  = 1;

    return id;
}

int apic_delete_timer(unsigned id) {
    if(id >= n_timers)
        return 0;
    timers[id].exists = 0;

    // check if we can free the end of the list 

    // end of the list: 
    // timers[timer_end -> n_timers-1].exist = 0
    unsigned timer_end = 0; 
    for(unsigned i = 0; i < n_timers; i++) {
        if(timers[i].exists)
            timer_end = i+1;
    } 

    if(n_timers != timer_end) {
        n_timers = timer_end;
        timers_realloc();
    }

    return 1;
}


__attribute__((interrupt)) 
void lapic_timer_handler(struct IFrame* frame) {
    (void) frame;
    ++apic_timer_clock_count;

    for(unsigned i = 0; i < n_timers; i++) {
        if(++timers[i].counter >= timers[i].period) {
            timers[i].func(timers[i].param);
            timers[i].counter = 0;
        }
    }


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

void apic_setup_clock(void) {
    log_info("setup local apic clock...");
    
// enable apic msr
    uint64_t IA32_APIC_BASE = read_msr(0x1b);    
    IA32_APIC_BASE |= (1 << 11);
    write_msr(0x1b, IA32_APIC_BASE);


    assert(apic_config != NULL);
    set_irq_handler(32, lapic_timer_handler);

    // enable apic and set spurious int to 0xff
    apic_config->spurious_interrupt_vector.reg = 0x100 | LAPIC_SPURIOUS_IRQ; 
    
    // masks the irq, one shot mode
    apic_config->LVT_timer.reg = 0x20000; 

    apic_config->timer_divide_configuration.reg = 3; // divide by 16

    hpet_prepare_wait_ms(1);

    apic_config->timer_initial_count.reg = UINT32_MAX;

    hpet_wait();


    uint32_t t = (UINT32_MAX - apic_config->timer_current_count.reg);

    apic_config->timer_initial_count.reg = t;

    // enable apic and set spurious int to 0xff
// unmask the IRQ, periodic mode, timer on irq 32
    apic_config->LVT_timer.reg = 0x20020;

    hpet_disable();

}

