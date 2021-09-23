#pragma once

#include <stddef.h>
#include <stdint.h>
#include "../debug/assert.h"


struct APICRegister 
{
    uint32_t reg;
    uint32_t reserved[3];
};
static_assert(sizeof(struct APICRegister) == 16);

struct APICConfig
{
    struct APICRegister reserved1[2];                         // 
    struct APICRegister LAPIC_ID;                             // RW
    struct APICRegister LAPIC_version;                        // R
    struct APICRegister reserved2[4];                         // 
    struct APICRegister task_priority;                        // RW
    struct APICRegister arbitration_priority;                 // R
    struct APICRegister processor_priority;                   // R
    struct APICRegister end_of_interrupt;                     // W
    struct APICRegister remote_read;                          // R
    struct APICRegister logical_destination;                  // RW
    struct APICRegister destination_format;                   // RW
    struct APICRegister spurious_interrupt_vector;            // RW
    struct APICRegister in_service[8];                        // R
    struct APICRegister trigger_mode[8];                      // R
    struct APICRegister interrupt_request[8];                 // R
    struct APICRegister error_status;                         // R
    struct APICRegister reserved3[6];                         // 
    struct APICRegister LVT_corrected_machine_check_interrupt;// RW
    struct APICRegister interrupt_command[2];                 // RW
    struct APICRegister LVT_timer;                            // RW
    struct APICRegister LVT_thermal_sensor;                   // RW
    struct APICRegister LVT_performance_monitoring_counters;  // RW
    struct APICRegister LVT_LINT0;                            // RW
    struct APICRegister LVT_LINT1;                            // RW
    struct APICRegister LVT_error;                            // RW
    struct APICRegister timer_initial_count;                  // RW
    struct APICRegister timer_current_count;                  // R
    struct APICRegister reserved4[4];                         // 
    struct APICRegister timer_divide_configuration;           // RW
    struct APICRegister reserved5;                            // 

};
static_assert(sizeof(struct APICConfig) == 0x400);


void apic_setup_clock(void);
uint64_t clock(void);
