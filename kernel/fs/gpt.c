#include "../lib/string.h"
#include "../lib/logging.h"
#include "../lib/utf16le.h"
#include "../lib/dump.h"
#include "../lib/assert.h"
#include "../lib/sprintf.h"

#include "../drivers/driver.h"
#include "../drivers/dev.h"
#include "../fs/vfs.h"

#include "gpt.h"


// return 0 if equals, non zero else
static int guidcmp(GUID const a, GUID const b) {
    return ((a.high ^ b.high) | (a.low ^ b.low));
}


struct gpt_partition_descriptor {
    GUID type_guid;
    GUID partition_guid;
    uint64_t begin;
    uint64_t end;
    uint64_t attributes;
    uint16_t  name[];
} __attribute__((packed));


static disk_part_t* partitions = NULL;
static unsigned n_partitions = 0;

static void register_partition(disk_part_t* p) {
    unsigned last = n_partitions++;

    partitions = realloc(partitions, n_partitions * sizeof(disk_part_t));
    

    partitions[last] = *p;

        
    log_info(
        "GPT Partition %s\n"
        "LBA begin:  %lx\n"
        "LBA end:    %lx\n"
        "attributes: %lx\n"
        "type:       %x\n"
        "guid:   %lx-%lx\n", 
        
        partitions[last].name,
        partitions[last].begin,
        partitions[last].end,
        partitions[last].attributes,
        partitions[last].type,
        partitions[last].guid.low,
        partitions[last].guid.high
    );

}


disk_part_t* find_partition(GUID guid) {

    for(unsigned i = 0; i < n_partitions; i++) {
        if(!guidcmp(partitions[i].guid, guid))
            return &partitions[i];
    }
    return NULL;
}


disk_part_t* search_partition(const char* name) {

    for(unsigned i = 0; i < n_partitions; i++)
        if(!strcmp(partitions[i].name, name))
            return &partitions[i];
    return NULL;
}

void gpt_remove_drive_parts(driver_t* driver) {
    assert(partitions);

    int n_removed = 0;

    for(unsigned i = 0; i < n_partitions; i++) {
        if(partitions[i].interface->driver == driver) {
            
            // the partition is mounted
            // somewhere
            if(partitions[i].mount_point) {
                log_warn("part %u is mounted on %s, let's unmount it", i, partitions[i].mount_point);
                vfs_unmount(partitions[i].mount_point);
            }

            n_removed++;
            n_partitions--;
            memmove(
                &partitions[i], 
                &partitions[i+1], 
                sizeof(partitions[i]) * (n_partitions - i)
            );
            
            i--;
        }
    }

    if(!n_partitions)
       ;// gpt_cleanup();
}

void gpt_cleanup(void) {
    if(partitions != NULL) {
        free(partitions);
        partitions = NULL;
    }
    n_partitions = 0;
}



static GUID makeGUID(
            uint32_t a, uint16_t b, 
            uint16_t c, uint16_t d, 
            uint64_t e) {
    return (GUID) {
        .low  = a | ((uint64_t)b << 32) | ((uint64_t)c << 48),
        .high = d | (e << 16)
    };
}


 uint32_t get_type(GUID guid) {
                                                              
    if     (!guidcmp(guid,makeGUID(0xC12A7328,0xF81F,0x11D2,0x4BBA,0x3bc93ec9a000)))
        return PARTITION_ESP;
    else if(!guidcmp(guid,makeGUID(0x0FC63DAF,0x8483,0x4772,0x798E,0xE47D47D8693D)))
        return PARTITION_LINUX_FS;
    else if(!guidcmp(guid,makeGUID(0x44479540,0xF297,0x41B2,0xF79A,0x8A45F0D531D1))
         || !guidcmp(guid,makeGUID(0x4F68BCE3,0xE8CD,0x4DB1,0xE796,0x09B784F9CAFB)))
        return PARTITION_LINUX_ROOT;
    else if(!guidcmp(guid,makeGUID(0xBC13C2FF,0x59E6,0x4262,0x52A3,0x72716FFD75B2)))
        return PARTITION_LINUX_BOOT;
    else if(!guidcmp(guid,makeGUID(0x0657FD6D,0xA4AB,0x43C4,0xE584,0x4F4F4BC83309)))
        return PARTITION_LINUX_SWAP;
    else if(!guidcmp(guid,makeGUID(0xE6D6D379,0xF507,0x44C2,0x3CA2,0x28F93D2A8F23)))
        return PARTITION_LINUX_LVM;
    else if(!guidcmp(guid,makeGUID(0x933AC7E1,0x2EB4,0x4F13,0x44B8,0x15F9AEE2140E)))
        return PARTITION_LINUX_HOME;
    else if(
           !guidcmp(guid,makeGUID(0xE3C9E316,0x0B5C,0x4DB8,0x7D81,0xAE1502F02DF9))
        || !guidcmp(guid,makeGUID(0xEBD0A0A2,0xB9E5,0x4433,0xC087,0xC79926B7B668))
        || !guidcmp(guid,makeGUID(0x5808C8AA,0x7E8F,0x42E0,0xD285,0xB3CF3404E9E1))
        || !guidcmp(guid,makeGUID(0xAF9B60A0,0x1431,0x4F62,0x68BC,0xAD694A711133))
        || !guidcmp(guid,makeGUID(0xDE94BBA4,0x06D1,0x4D40,0x6AA1,0xACD67901D5BF))
        || !guidcmp(guid,makeGUID(0x37AFFC90,0xEF7D,0x4E96,0xC391,0x74B155E07A2D))
        || !guidcmp(guid,makeGUID(0xE75CAF8F,0xF680,0x4CEE,0xA3AF,0x2DFC6EE501B0))
        || !guidcmp(guid,makeGUID(0x558D43C5,0xA1AC,0x43C0,0xC8AA,0xD123292B47D1))
        )
        return PARTITION_WINDOWS;
        
    return PARTITION_UNKNOWNED;
}


void gpt_scan(const struct storage_interface* sti) {

    void* buffer = malloc(1 << sti->lbashift);
    
    // LBA 1: Partition Table Header
    sti->read(sti->driver, 1, buffer, 1);

    // magic
    if(strcmp(buffer+0, "EFI PART")) {
        log_warn("no GPT table found in device %s.", 
                 sti->driver->device->name.ptr);
        return;
    }


    // usually equal to 2
    uint64_t partition_entry_array_lba = *(uint64_t*)(buffer+0x48);

    unsigned n_partitions = *(uint32_t*)(buffer+0x50);

    unsigned partition_entry_size = *(uint32_t*)(buffer+0x54);

    unsigned size = (partition_entry_size * n_partitions);

    buffer = realloc(buffer, size);


    // LBA 1: Partition Entries
    sti->read(sti->driver, partition_entry_array_lba,
              buffer, size >> sti->lbashift);


    // temporary pointer of gpt partition entry
    void* ptr = buffer;

    for(unsigned i = 0; i < n_partitions; 
            i++, ptr += partition_entry_size) {
        
        struct gpt_partition_descriptor* entry = ptr;

        if(entry->type_guid.low  == 0 &&
           entry->type_guid.high == 0)
            continue;
        
        disk_part_t p = (disk_part_t) {
            .begin      = entry->begin,
            .end        = entry->end,
            .attributes = entry->attributes,
            .type       = get_type(entry->type_guid),
            .id         = i,

            .guid = entry->partition_guid,
            .interface = sti,
        };


        assert(strlen(sti->driver->device->name.ptr) + 2 + 4 <= sizeof(p.sysname));

        sprintf(p.sysname, "%sp%u", sti->driver->device->name.ptr, i+1);

        utf16le2ascii(p.name, entry->name, 35);

        register_partition(&p);
    }
    
    free(buffer);
}