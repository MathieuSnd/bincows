#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../common.h"

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



// plagia from 
// https://github.com/DorianXGH/Lucarnel/blob/master/src/includes/acpi.h
//
struct ACPISDTHeader
{
    uint8_t signature[4]; // signature of the table
    uint32_t length;      // length of the table
    uint8_t revision;
    uint8_t checksum;
    uint8_t OEMID[6];
    uint8_t OEMtableID[8];
    uint32_t OEMrevision;
    uint32_t creatorID;
    uint32_t creator_revision;
} __packed;
