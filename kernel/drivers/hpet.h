#pragma once

#include <stdint.h>

void hpet_init(void);
void hpet_prepare_wait_ns(uint64_t);

// enables HPET, busy wait until the main counteur
// reaches 0, and returns without disabling HPET!  
void hpet_wait(void);

void hpet_enable(void);
void hpet_disable(void);

void hpet_enable_irq(uint8_t vector);
void hpet_mask_irq(void);
