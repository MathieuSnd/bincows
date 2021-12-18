#pragma once

#include <stddef.h>
#include <stdint.h>
#include "../lib/assert.h"

#define LAPIC_SPURIOUS_IRQ 0xff
#define INVALID_TIMER_ID ((unsigned)-1)

void apic_setup_clock(void);
uint64_t clock(void);

typedef void (* timer_callback_t)(void *);

// return the id of the created timer
unsigned apic_create_timer(timer_callback_t, int millisecs, void* param);

// zero if it couldn't be deleted
int apic_delete_timer(unsigned id);

