#pragma once

#include <stdint.h>
#include <stddef.h>
#include "../../lib/assert.h"


struct PCIE_busgroup {
    void* address; // Base address 
        // of enhanced configuration mechanism
    uint16_t group; // PCI Segment Group Number
    uint8_t  start_bus; // Start PCI bus number 
        // decoded by this host bridge
    uint8_t end_bus; // End PCI bus number 
        // decoded by this host bridge
    uint32_t reserved;
} __attribute__((packed));

static_assert_equals(sizeof(struct PCIE_busgroup), 16);


#define PCIE_SUPPORTED_SEGMENT_GROUPS 2
struct PCIE_Descriptor {
    size_t size;
    struct PCIE_busgroup array[2];
    // we only handle PCIE devices with only 4 segment groups max
};


// defined in pcie.c
extern struct PCIE_Descriptor pcie_descriptor;


struct pcie_dev* pcie_scan(unsigned* size);


