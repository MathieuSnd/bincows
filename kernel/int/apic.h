#pragma once

#include <stddef.h>
#include <stdint.h>
#include "../lib/assert.h"

#define LAPIC_SPURIOUS_IRQ 0xff
#define INVALID_TIMER_ID ((unsigned)-1)

void apic_setup_clock(void);
uint64_t clock(void);

// return the id of the created timer
unsigned apic_create_timer(void (*)(void), int millisecs);

// zero if it couldn't be deleted
int apic_delete_timer(unsigned id);

