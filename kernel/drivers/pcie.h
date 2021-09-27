#pragma once

#include <stdint.h>


struct PCIE_config_space_descriptor {
    void* address; // Base address 
        // of enhanced configuration mechanism
    uint16_t group; // PCI Segment Group Number
    uint8_t  start_bus; // Start PCI bus number 
        // decoded by this host bridge
    uint8_t end_bus; // End PCI bus number 
        // decoded by this host bridge
    uint32_t reserved;
};



struct PCIE_Descriptor {
    size_t size;
    struct PCIE_config_space_descriptor array[32];
};


#ifndef PCIE_C
extern struct PCIE_Descriptor pcie_descriptor;
#else
struct PCIE_Descriptor pcie_descriptor = {0, {0}};
#endif//PCIE_C
