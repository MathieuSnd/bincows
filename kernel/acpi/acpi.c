#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


#include "acpi.h"
#include "acpitables.h"

#include "../lib/string.h"
#include "../int/apic.h"
#include "../lib/logging.h"
#include "../lib/panic.h"

#include "../memory/vmap.h"
#include "../memory/heap.h"
#include "../memory/paging.h"

#include "../drivers/pcie/scan.h"

static void* apic_config_base, *hpet_config_space;

// defined in pcie/scan.c
extern struct PCIE_Descriptor pcie_descriptor;

struct table_d {
    char signature[4];
    const void* vaddr;
};

int table_list_size = 0;
struct table_d* table_list;



static void add_table(const char signature[4], const void* vaddr) {
    table_list = realloc(table_list, 
                    sizeof(struct table_d) * (table_list_size+1));
    


    struct table_d* entry = &table_list[table_list_size++];

    memcpy(entry->signature, signature, 4);
    entry->vaddr = vaddr;
}


void acpi_scan_clean(void) {
    free(table_list);
}

const void* acpi_get_table(const char * signature, size_t c) {
    for(int i = 0; i < table_list_size; i++) {
        if(!strncmp(signature, table_list[i].signature, 4))
            if(c-- == 0)
                return  table_list[i].vaddr;
    }
    return NULL;
}




static void parse_madt(const struct MADT* table);
static void parse_hpet(const struct HPET* table);
static void parse_fadt(const struct FADT* table);
static void parse_pcie(const struct PCIETable* table);
static void parse_xsdt(struct XSDT* xsdt);


void acpi_scan(const void* rsdp) {
    const struct RSDPDescriptor20* rsdpd = rsdp;

    if(rsdpd->firstPart.revision < 2)
        panic("unsuported ACPI revision");


    
    // checksum for xsdt
    if(rsdpd->length != sizeof(struct RSDPDescriptor20))
        panic("bad ACPI RSDP size");
    
    if(memsum(rsdpd, rsdpd->length))
        panic("failed ACPI RSDP checksum");

    parse_xsdt(translate_address((void*)rsdpd->xsdtAddress));
}


static void parse_xsdt(struct XSDT* xsdt) {


    size_t n_entries = (xsdt->header.length - sizeof(xsdt->header)) / sizeof(void*);

    log_debug("%u ACPI entries found:", n_entries);

    bool madt_parsed = false,
         hpet_parsed = false,
         pcie_parsed = false,
         fadt_parsed = false;

    for(size_t i = 0; i < n_entries; i++) {
        const struct ACPISDTHeader* table = translate_address(xsdt->entries[i]);
        if(memsum(table, table->length)) {
            log_warn("ACPI %s table checksum failed", table->signature.arg);
            panic("ACPI error");
        }

        log_info("    - %4s", table->signature.arg);

        add_table(table->signature.arg, table);

        switch(table->signature.raw) {
        case MADT_SIGNATURE:
            parse_madt((const struct MADT *)table);
            madt_parsed = true;
            break;
        case FADT_SIGNATURE:
            parse_fadt((struct FADT*)table);
            fadt_parsed = true;
            break;
        case HPET_SIGNATURE:
            parse_hpet((const void*)table);
            hpet_parsed = true;
            break;
        case PCIE_SIGNATURE:
            parse_pcie((const struct PCIETable *)table);
            pcie_parsed = true;
        default:
            break;
        }
    }

    
    if(!madt_parsed)
        panic("could not find ACPI MADT table");
    if(!hpet_parsed)
        panic("could not find ACPI HPET table");
    if(!pcie_parsed)
        panic("could not find ACPI PCIE table");

    if(!fadt_parsed) // needed by LAI
        log_warn("could not find ACPI FADT table");


        
    // mmios to map: HPET, PCIE
    // cache disable
    map_pages((uint64_t)apic_config_base, APIC_VADDR, 1,
                                 PRESENT_ENTRY | PCD | PL_XD | PL_RW);
    map_pages((uint64_t)hpet_config_space, HPET_VADDR, 1,
                                 PRESENT_ENTRY | PCD | PL_XD | PL_RW);
}


static void parse_hpet(const struct HPET* table) {
    hpet_config_space = (void*)table->address;
}

static void parse_madt(const struct MADT* table) {
    // checksum is already done

    apic_config_base = (void*)(uint64_t)table->lAPIC_address;
    const void* ptr = table->entries;
    const void* end = (const void*)table + table->header.length;

/// dont override the override lapic or io apic entry
    bool override_lapic_passed  = false;
    bool override_ioapic_passed = false;
    (void)override_lapic_passed;    
    (void)override_ioapic_passed;
    

    while(ptr < end) {
        const struct MADTEntryHeader* entry_header = ptr;

        switch(entry_header->type) {
            case APIC_TYPE_LAPIC:
                    // const struct MADT_lapic_entry* entry = ptr;
                break;
            case APIC_TYPE_IO_APIC:
                    // const struct MADT_ioapic_entry* entry = ptr;
                break;
            case APIC_TYPE_IO_INTERRUPT_SOURCE_OVERRIDE:
                    // const struct MADT_ioapic_interrupt_source_override_entry* entry = ptr;
                break;
            case APIC_TYPE_IO_NMI:
                    // const struct MADT_IO_NMI_entry* entry = ptr;
                break;
            case APIC_TYPE_LOCAL_NMI:
                    // const struct MADT_LOCAL_NMI_entry* entry = ptr;
                break;
            case APIC_TYPE_LAPIC_ADDRESS_OVERRIDE:
                    // const struct MADT_LAPIC_address_override_entry* entry = ptr;
                break;
            case APIC_TYPE_LAPICX2:
                    // const struct MADT_lapicx2_entry* entry = ptr;
                break;
            default:
                log_warn("invalid APIC MADT entry %u", entry_header->type);
        }
        ptr += entry_header->length;
    }
}


static void parse_pcie(const struct PCIETable* table) {
    // fill the pcie driver's descriptor 
    size_t size = (table->header.length-sizeof(struct ACPISDTHeader)-8);

    pcie_descriptor.size = size / sizeof(struct PCIE_busgroup);
    
    
    if(pcie_descriptor.size > PCIE_SUPPORTED_SEGMENT_GROUPS)
        panic("ACPI: unsuported pcie segment group count");

    memcpy(pcie_descriptor.array, table->segments, size);
}

static void parse_fadt(const struct FADT* table) {
    // just extract DSDT and FACS talbes
    uint64_t dsdt = table->DSDT;
    uint64_t facs = table->firmware_ctrl;

    if(!dsdt) // > 4 GB
        dsdt = table->x_DSDT;
    if(!facs) // > 4 GB
        facs = table->x_firmware_control;

    if(!dsdt) {
        panic("could not find DSDT");
    }
    if(!facs) {
        panic("could not find FACS");
    }

    add_table("FACS", translate_address((void*)facs));
    add_table("DSDT", translate_address((void*)dsdt));
}

