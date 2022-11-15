#include <stdint.h>

#include "idt.h"
#include "irq.h"
#include "apic.h"

#include "../lib/assert.h"
#include "../lib/logging.h"
#include "../lib/sprintf.h"
#include "../lib/time.h"
#include "../memory/vmap.h"
#include "../drivers/hpet.h"
#include "../lib/string.h"
#include "../memory/heap.h"
#include "../sched/sched.h"



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


volatile struct APICConfig* apic_config = (void *)APIC_VADDR;



// acknowledge an apic IRQ
void apic_eoi(void) {
    apic_config->end_of_interrupt.reg = 0;
}


static volatile uint64_t timer_offset = 0;


static uint64_t lapic_period = 0;
static uint64_t lapic_max    = 0;


void lapic_timer_handler(void* arg) {
    (void)arg;

    timer_offset += 1000lu * 1000lu * 1000lu * LAPIC_IRQ_FREQ;


    wakeup_threads();

    apic_eoi();
    
    if(timer_irq_should_schedule())
        schedule();
}  


static const uint64_t timer_mult = 1;
static uint64_t old_clk = 0;

uint64_t clock_ns(void)  {
    
    // uninitialized
    if(lapic_period == 0)
        return lapic_period;

    
    uint64_t rf = get_rflags();

    _cli();


    volatile uint32_t snap = apic_config->timer_current_count.reg;

    uint64_t clk = timer_offset + (lapic_max - snap) * lapic_period / timer_mult;


    set_rflags(rf);

    //log_info("%lu > %lu", clk, old_clk);
    if(clk < old_clk) {
        // the timer has ticked.
        // this only work if the maximum time with
        // interrupts disabled is inferior to one
        // timer tick
        clk += 1000lu * 1000lu * 1000lu * LAPIC_IRQ_FREQ;

        if(clk < old_clk)
            // well...
            return old_clk;
    }


    old_clk = clk;
    return clk;
}


extern uint64_t read_msr(uint32_t address);
extern void     write_msr(uint32_t address, uint64_t value);


uint32_t get_lapic_id(void) {
    return apic_config->LAPIC_ID.reg;
}


void apic_setup_clock(void) {
    log_info("setup local apic clock...");
    
// enable apic msr
    uint64_t IA32_APIC_BASE = read_msr(0x1b);    
    IA32_APIC_BASE |= (1 << 11);
    write_msr(0x1b, IA32_APIC_BASE);


    assert(apic_config != NULL);

    register_irq(IRQ_APIC_TIMER, (void*)lapic_timer_handler, NULL);

    // enable apic and set spurious int to 0xff
    apic_config->spurious_interrupt_vector.reg = 0x100 | IRQ_LAPIC_SPURIOUS; 
    
    // masks the irq, one shot mode
    apic_config->LVT_timer.reg = 0x20000; 

    apic_config->timer_divide_configuration.reg = 0; // divide by 2


    hpet_prepare_wait_ns(1000llu * 1000llu * 1000llu / LAPIC_IRQ_FREQ);



    apic_config->timer_initial_count.reg = UINT32_MAX;

    hpet_wait();


    uint32_t t = (UINT32_MAX - apic_config->timer_current_count.reg);


    apic_config->timer_initial_count.reg = lapic_max = t;


    lapic_period = (1000lu * 1000lu * 1000lu / LAPIC_IRQ_FREQ * timer_mult) / t;




    // enable apic and set spurious int to 0xff
// unmask the IRQ, periodic mode, timer on irq 48
    apic_config->LVT_timer.reg = 0x20000 | 48;

    hpet_disable();
}

