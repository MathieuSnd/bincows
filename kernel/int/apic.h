#pragma once

#include <stddef.h>
#include <stdint.h>
#include "../lib/assert.h"

#define IRQ_LAPIC_SPURIOUS 0xff
#define IRQ_APIC_TIMER 48


// in Hz
#define LAPIC_IRQ_FREQ 50


void apic_setup_clock(void);
uint64_t clock_ns(void);

// acknowledge an apic IRQ
void apic_eoi(void);


uint32_t get_lapic_id(void);

