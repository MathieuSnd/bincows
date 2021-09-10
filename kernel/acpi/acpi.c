#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "acpi.h"
#include "../common.h"
#include "../debug/assert.h"
#include "../debug/dump.h"
#include "../klib/sprintf.h"
#include "acpitables.h"



bool checksum(void* table, size_t size) {
    uint8_t sum = 0;
    uint8_t* raw = table;

    for(size_t i;size > 0; --size) {
        sum += raw[i++];
    }
    return sum == 0;
}


typedef uint64_t rsdp_entry_t;



void read_acpi_tables(void* rsdp_location) {
    struct RSDPDescriptor20* rsdpd = rsdp_location;


    dump(rsdpd, sizeof(struct RSDPDescriptor20), 8, DUMP_HEX8);

    assert(rsdpd->firstPart.revision >= 2);
    
    // checksum for xsdt
    assert(rsdpd->length == sizeof(rsdpd));
    assert(checksum(rsdpd, rsdpd->length));


    // lets parse it!!
    const struct ACPISDTHeader* xsdt = (void *)rsdpd->xsdtAddress;

    size_t n_entries = (xsdt->length - sizeof(xsdt)) / sizeof(rsdp_entry_t);

    rsdp_entry_t* table = (xsdt + 1);

    for(int i = 0; i < n_entries; i++) {


        kprintf("%3u: %s\n", table[i])
        //table[i]
    }
}
