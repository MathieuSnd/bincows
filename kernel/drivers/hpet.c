#include <stdint.h>
#include <stddef.h>
#include "hpet.h"
#include "../memory/vmap.h"
#include "../lib/assert.h"
#include "../lib/logging.h"
/**
000-007h General Capabilities and ID Register Read Only
008-00Fh Reserved
010-017h General Configuration Register       Read-Write
018-01Fh Reserved
020-027h General Interrupt Status Register    Read/Write Clear
028-0EFh Reserved
0F0-0F7h Main Counter Value Register Read/Write
0F8-0FFh Reserved
100-107h Timer 0 Configuration and Capability Register Read/Write
108-10Fh Timer 0 Comparator Value Register Read/Write
110-117h Timer 0 FSB Interrupt Route Register Read/Write
118-11Fh Reserved
120-127h Timer 1 Configuration and Capability Register Read/Write
128-12Fh Timer 1 Comparator Value Register Read/Write
130-137h Timer 1 FSB Interrupt Route Register Read/Write
138-13Fh Reserved
140-147h Timer 2 Configuration and Capability Register Read/Write
148-14Fh Timer 2 Comparator Value Register Read/Write
150-157h Timer 2 FSB Interrupt Route Register Read/Write
158-15Fh Reserved
160-3FFh Reserved for Timers 3-31 
 */
struct HPET_MMIO {
    volatile const uint64_t capabilities;
    const uint64_t reserved0;
    volatile uint64_t       general_config;
    uint64_t reserved1;
    volatile uint64_t irqstate;
    uint8_t reserved2[0xf0-0x28];
    volatile uint64_t main_counter_value;
    uint64_t reserved3;
    
    volatile uint64_t timer0_config; 
    volatile uint64_t timer0_conparator_value; 
    volatile uint64_t timer0_FSB_interrupt_route; 

    uint8_t reserved4[0x400 - 0x118];
};

static_assert(sizeof(struct HPET_MMIO) == 0x400);

struct HPET_MMIO* const base = (void *)HPET_VIRTUAL_ADDRESS;


static uint32_t hpet_period = 0;

void hpet_init(void) {
    log_debug("init hpet...");
    
    hpet_period = base->capabilities >> 32;
}

void hpet_prepare_wait_ms(unsigned ms) {
    hpet_period = base->capabilities >> 32;

// deactivate hpet timer
    base->general_config &= ~1;
    base->main_counter_value = - ((uint64_t)ms * 0xe8d4a51000llu / hpet_period);
}
//t * 1000 / (T * 1e15) * 

void hpet_wait() {
    base->general_config |= 1;

    while(base->main_counter_value > 0x1000) {
            
    }
}

void hpet_disable(void) {
    base->general_config &= ~1llu;
}


