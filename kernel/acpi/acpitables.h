#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../lib/common.h"
#include "../drivers/pcie/scan.h"

struct RSDPDescriptor {
    uint8_t  signature[8];
    uint8_t  checksum;
    uint8_t  OEMID[6];
    uint8_t  revision;
    uint32_t rsdtAddress;
} __packed;

struct RSDPDescriptor20 {
    struct RSDPDescriptor firstPart;
    
    uint32_t length;
    uint64_t xsdtAddress;
    uint8_t  extendedChecksum;
    uint8_t  reserved[3];
} __packed;


union acpi_signature {
    char arg[4];
    uint32_t raw;
} __packed;


#define MADT_SIGNATURE 0x43495041
#define FACP_SIGNATURE 0x50434146
#define PCIE_SIGNATURE 0x4746434d
#define HPET_SIGNATURE 0x54455048



// plagia from 
// https://github.com/DorianXGH/Lucarnel/blob/master/src/includes/acpi.h
//
struct ACPISDTHeader {
    union acpi_signature signature;
    
    uint32_t length;      // length of the table
    uint8_t  revision;
    uint8_t  checksum;
    uint8_t  OEMID[6];
    uint8_t  OEMtableID[8];
    uint32_t OEMrevision;
    uint32_t creatorID;
    uint32_t creator_revision;
} __packed;

// end


struct XSDT {
    struct ACPISDTHeader header;
    struct ACPISDTHeader* entries[];
} __packed;


struct RSDT {
    struct ACPISDTHeader header;
    uint32_t entries[];
} __packed;


struct MADTEntryHeader {
    uint8_t type;
    uint8_t length;
} __packed;


struct MADT_lapic_entry {
    struct MADTEntryHeader header;
    uint8_t  proc_apic_ID;
    uint8_t  procID;
    uint32_t flags; // bit0: enabled
} __packed;


struct MADT_ioapic_entry {
    struct MADTEntryHeader header;
    uint8_t  id;
    uint8_t  reserved;
    uint32_t address;
    uint32_t global_system_interrupt_base;
} __packed;


struct MADT_ioapic_interrupt_source_override_entry {
    struct MADTEntryHeader header;
    uint8_t  bus_source;
    uint8_t  irq_source;
    uint32_t global_system_interrupt;
    uint16_t flags;
} __packed;


struct MADT_IO_NMI_entry {
    struct MADTEntryHeader header;
    uint8_t  source;
    uint8_t  reserved;
    uint32_t global_system_interrupt;
} __packed;


struct MADT_LOCAL_NMI_entry {
    struct MADTEntryHeader header;
    uint8_t  procID;
    uint16_t flags;
    uint8_t  lint; // 0 or 1
} __packed;


struct MADT_LAPIC_address_override_entry {
    struct MADTEntryHeader header;
    uint8_t procID;
    uint8_t flags;
    uint32_t lint; // 0 or 1
} __packed;


struct MADT_lapicx2_entry {
    struct MADTEntryHeader header;
    uint8_t  proc_lapic_ID;
    uint64_t flags; // bit0: enabled
    uint32_t acpi_id;
} __packed;


struct MADT {
    struct ACPISDTHeader header;
    uint32_t lAPIC_address;
    uint32_t flags;
    struct MADTEntryHeader* entries[];
} __packed;


static_assert(
    sizeof(struct PCIE_busgroup) == 16);

struct PCIETable {
    struct ACPISDTHeader header;
    uint64_t reserved0;

    struct PCIE_busgroup segments[];
} __packed;


// MADT entry types
#define APIC_TYPE_LAPIC 0
#define APIC_TYPE_IO_APIC 1
#define APIC_TYPE_IO_INTERRUPT_SOURCE_OVERRIDE 2
#define APIC_TYPE_IO_NMI 3
#define APIC_TYPE_LOCAL_NMI 4
#define APIC_TYPE_LAPIC_ADDRESS_OVERRIDE 5
#define APIC_TYPE_LAPICX2 9


 
struct HPET
{
    struct ACPISDTHeader header;
    uint8_t hardware_rev_id;
    uint8_t comparator_count:5;
    uint8_t counter_size:1;
    uint8_t reserved:1;
    uint8_t legacy_replacement:1;
    uint16_t pci_vendor_id;
    
    uint8_t address_space_id;    // 0 - system memory, 1 - system I/O
    uint8_t register_bit_width;
    uint8_t register_bit_offset;
    uint8_t reserved2;
    uint64_t address;

    uint8_t hpet_number;
    uint16_t minimum_tick;
    uint8_t page_protection;
} __attribute__((packed));