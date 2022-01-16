#include "../lib/string.h"
#include "../lib/logging.h"
#include "../lib/utf16le.h"
#include "../lib/dump.h"

#include "../drivers/driver.h"
#include "../drivers/dev.h"

#include "gpt.h"

struct gpt_partition_descriptor {
    GUID type_guid;
    GUID partition_guid;
    uint64_t begin;
    uint64_t end;
    uint64_t attributes;
    wchar_t  name[];
} __attribute__((packed));

static partition_t* partitions = NULL;
static unsigned n_partitions = 0;

static void register_partition(partition_t p) {
    unsigned last = n_partitions++;

    partitions = realloc(partitions, n_partitions * sizeof(partition_t));
    

    partitions[last] = p;
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

    log_warn(
        "n_partitions=%lx, partition_entry_array_lba=%lx, "
        "partition_entry_size=%x", 

        n_partitions,
        partition_entry_array_lba,
        partition_entry_size
    );

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

        char* name[36];

        utf16le2ascii(name, entry->name, 35);

        log_info(
            "GPT Partition %s\n"
            "LBA begin:  %lx\n"
            "LBA end:    %lx\n"
            "attributes: %lx\n", 
            
            name,
            entry->begin,
            entry->end,
            entry->attributes
        );
    }
    
    
}