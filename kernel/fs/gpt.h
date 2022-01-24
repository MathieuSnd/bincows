#pragma once

#include "../drivers/storage_interface.h"


typedef struct {
    uint64_t low, high;
} __attribute__((packed)) GUID;

typedef struct disk_part {
    const struct storage_interface* __restrict__ interface;
    uint32_t type;
    uint32_t id;
    GUID guid;

    uint64_t begin;
    uint64_t end;

    uint64_t attributes;
    
    // null terminated
    char     name[36];
} disk_part_t;

inline 
static unsigned __attribute__((pure)) block_size(disk_part_t* part) {
    return 1 << part->interface->lbashift;
}


#define PARTITION_UNKNOWNED 0

// efi system partition
#define PARTITION_ESP 1 
#define PARTITION_BIOS 2 
#define PARTITION_WINDOWS 3 
#define PARTITION_LINUX_FS 4
#define PARTITION_LINUX_ROOT 4
#define PARTITION_LINUX_BOOT 5
#define PARTITION_LINUX_SWAP 6
#define PARTITION_LINUX_HOME 7
#define PARTITION_LINUX_LVM 8


void gpt_scan(const struct storage_interface* sti);


disk_part_t* find_partition(GUID guid);
disk_part_t* search_partition(const char* name);

// release partition information memory
void gpt_cleanup(void);

