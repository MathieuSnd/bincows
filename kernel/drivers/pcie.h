#pragma once

#include <stdint.h>
#include <stddef.h>
#include "../lib/assert.h"

struct PCIE_segment_group_descriptor {
    void* address; // Base address 
        // of enhanced configuration mechanism
    uint16_t group; // PCI Segment Group Number
    uint8_t  start_bus; // Start PCI bus number 
        // decoded by this host bridge
    uint8_t end_bus; // End PCI bus number 
        // decoded by this host bridge
    uint32_t reserved;
} __attribute__((packed));

static_assert(sizeof(struct PCIE_segment_group_descriptor) == 16);


#define PCIE_SUPPORTED_SEGMENT_GROUPS 2
struct PCIE_Descriptor {
    size_t size;
    struct PCIE_segment_group_descriptor array[PCIE_SUPPORTED_SEGMENT_GROUPS];
    // we only handle PCIE devices with only 4 segment groups max
};


// defined in pcie.c
extern struct PCIE_Descriptor pcie_descriptor;


void pcie_init(void);
void pcie_scan(void);
void pcie_init_devices(void);



typedef void (*driver_init_fun)(void* config_space); 
typedef void (*driver_callback)(void); 

struct driver_descriptor {
    driver_init_fun install;
    driver_callback remove;
    
    const char* driver_name;
    uint32_t status;

    void* driver_data;
    size_t driver_data_len;
};


/**
 * PCIE drivers interfaces:
 * 
 * provide void init(void* config_space_base)
 * 
 * functions to call:
 *      register_irq(unsigned)
 *      unregister_irq(unsigned)
 * 
 *      int  register_timer(void callback(void), unsigned period)
 *      void unregister_timer(int timer_id)
 *     
 */

