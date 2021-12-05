#pragma once

#include <stdint.h>
#include <stddef.h>

struct PCIE_segment_group_descriptor {
    void* address; // Base address 
        // of enhanced configuration mechanism
    uint16_t group; // PCI Segment Group Number
    uint8_t  start_bus; // Start PCI bus number 
        // decoded by this host bridge
    uint8_t end_bus; // End PCI bus number 
        // decoded by this host bridge
    uint32_t reserved;
};


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


typedef void (*driver_init_fun)(void* config_space); 
typedef void (*driver_callback)(void); 

struct driver_descriptor {
    driver_init_fun install;
    driver_callback remove;
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

