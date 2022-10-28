#ifndef BOOT_INTERFACE_H
#define BOOT_INTERFACE_H

#include <stddef.h>

#include "../fs/gpt.h"

// this file descibres the boot interface structure
// the the kernel needs in order to run

enum mmape_type {
    USABLE,
    KERNEL,
    BOOTLOADER_RECLAIMABLE,
    ACPI_RECLAIMABLE,
    ACPI_NVS,
    MMIO
};


struct mmape {
    // physical base address
    uint64_t pbase;
    // length in pages
    uint32_t length;

    enum mmape_type type;
};

struct boot_interface {
    // callback function to print strings onto a console
    // ignored if NULL
    // this function should be valid at least the GDT update
    void (*console_write)(const char *string, size_t length);

    // physical address of the RSDP ACPI table base
    void* rsdp_paddr;

    // kernel symbols file base
    // ignored if NULL
    void* kernel_symbols;

    // reutrn the next mmap entry
    // when the map end is reached, 
    // return NULL.
    // this function is only called in early memory context
    struct mmape* (*mmap_get_next)(void);
    
    // number of entries
    int mmap_entries;

    // all 0 if unavailable
    GUID boot_volume_guid;


};


#endif// BOOT_INTERFACE_H