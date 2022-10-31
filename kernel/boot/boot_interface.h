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
    ACPI_NVS
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
    uint64_t rsdp_paddr;

    // kernel symbols file base
    // ignored if NULL
    void* kernel_symbols;

    // reutrn the next mmap entry
    // when the map end is reached, 
    // return NULL. The next call wraps and 
    // returns the first entry
    //
    // this function is only called in early memory context:
    // before bootloader/ACPI memory reclaim.
    //
    // the entries returned by the function describe memory
    // ranges and ACPI NVS regions. It does not include MMIOs.
    struct mmape* (*mmap_get_next)(void);


    // physical address of the framebuffer
    // or 0 if no framebuffer installed
    uint64_t framebuffer_paddr;
    unsigned framebuffer_width;
    unsigned framebuffer_height;
    unsigned framebuffer_pitch;
    unsigned framebuffer_bpp;

    // byte size of the framebuffer
    // it should be paged-aligned
    size_t   framebuffer_size;

    
    // all 0 if unavailable
    GUID boot_volume_guid;


    char* bootloader_brand;
    char* bootloader_version;
};


#endif// BOOT_INTERFACE_H