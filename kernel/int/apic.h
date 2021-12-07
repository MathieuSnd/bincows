#pragma once

#include <stddef.h>
#include <stdint.h>
#include "../lib/assert.h"

#define LAPIC_SPURIOUS_IRQ 0xff


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


void apic_setup_clock(void);
uint64_t clock(void);
