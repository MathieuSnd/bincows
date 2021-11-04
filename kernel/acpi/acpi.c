#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "acpi.h"
#include "../common.h"
#include "../debug/assert.h"
#include "../debug/dump.h"
#include "../klib/sprintf.h"
#include "../klib/string.h"
#include "acpitables.h"
#include "../int/apic.h"


extern struct APICConfig* apic_config;

static bool __ATTR_PURE__ checksum(const void* table, size_t size) {
    uint8_t sum = 0;
    const uint8_t* raw = table;

    for(size_t i;size > 0; --size) {
        sum += raw[i++];
    }
    return sum == 0;
}

static void parse_madt(const struct MADT* table);
static void parse_hpet(const struct HPET* table);
static void parse_fadt(const struct ACPISDTHeader* table);


#define MADT_SIGNATURE 0x43495041
#define FACP_SIGNATURE 0x50434146
#define HPET_SIGNATURE 0x54455048

void read_acpi_tables(const void* rsdp_location) {
    const struct RSDPDescriptor20* rsdpd = rsdp_location;


    assert(rsdpd->firstPart.revision >= 2);
    
    // checksum for xsdt
    assert(rsdpd->length == sizeof(struct RSDPDescriptor20));
    assert(checksum(rsdpd, rsdpd->length));


    // lets parse it!!
    const struct XSDT* xsdt = (void *)rsdpd->xsdtAddress;


    size_t n_entries = (xsdt->header.length - sizeof(xsdt->header)) / sizeof(void*);


    bool madt_parsed = false,
         hpet_parsed = false,
         fadt_parsed = false;

    for(size_t i = 0; i < n_entries; i++) {
        const struct ACPISDTHeader* table = xsdt->entries[i];
        assert(checksum(&table, table->length));

        switch(table->signature.raw) {
        case MADT_SIGNATURE:
            parse_madt((const struct MADT *)table);
            madt_parsed = true;
            break;
        case FACP_SIGNATURE:
            parse_fadt(table);
            fadt_parsed = true;
            break;
        case HPET_SIGNATURE:
            parse_hpet(table);
            hpet_parsed = true;
            break;
        default:
            break;
        }
        kprintf("%3u: %4s\n", i, table->signature.arg);
    }

    
    assert(madt_parsed);
    //assert(hpet_parsed);
    assert(fadt_parsed);
}

static void parse_hpet(const struct HPET* table) {
    
}

static void parse_madt(const struct MADT* table) {
    // checksum is already done

    apic_config = (void*)(uint64_t)table->lAPIC_address;
    const void* ptr = table->entries;
    const void* end = (const void*)table + table->header.length;

/// dont override the override lapic or io apic entry
    bool override_lapic_passed  = false;
    bool override_ioapic_passed = false;
    (void)override_lapic_passed;    
    (void)override_ioapic_passed;
    

    kprintf("table size: %u\n", end-ptr);
    while(ptr < end) {
    //for(int i = 10; i>0; i--) {
        const struct MADTEntryHeader* entry_header = ptr;



        switch(entry_header->type) {
            case APIC_TYPE_LAPIC:
                {
                    // const struct MADT_lapic_entry* entry = ptr;
                    
                }
                break;
            case APIC_TYPE_IO_APIC:
                {
                    // const struct MADT_ioapic_entry* entry = ptr;
                }
                break;
            case APIC_TYPE_IO_INTERRUPT_SOURCE_OVERRIDE:
                {
                    // const struct MADT_ioapic_interrupt_source_override_entry* entry = ptr;
                }
                break;
            case APIC_TYPE_IO_NMI:
                {
                    // const struct MADT_IO_NMI_entry* entry = ptr;
                }
                break;
            case APIC_TYPE_LOCAL_NMI:
                {
                    // const struct MADT_LOCAL_NMI_entry* entry = ptr;
                }
                break;
            case APIC_TYPE_LAPIC_ADDRESS_OVERRIDE:
                {
                    // const struct MADT_LAPIC_address_override_entry* entry = ptr;
                }
                break;
            case APIC_TYPE_LAPICX2:
                {
                    // const struct MADT_lapicx2_entry* entry = ptr;
                }
                break;
            default:
                kprintf("WARNING: invalid APIC MADT entry %u\n", entry_header->type);


        }

        kprintf("entry: type %u ; size %u\n", entry_header->type,entry_header->length);

        ptr += entry_header->length;
    }
}


static void parse_fadt(const struct ACPISDTHeader* table) {
    (void) table;
}

