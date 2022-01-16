#pragma once

#include "../drivers/storage_interface.h"


typedef struct {
    uint64_t low, high;
} GUID;

typedef struct partition {
    const struct storage_interface* interface;
    uint32_t type;
    GUID partition_guid;

    uint64_t begin;
    uint64_t end;


    uint64_t attributes;
    
    // null terminated
    char     name[36];
} partition_t;

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


partition_t* find_partition(GUID guid);

// release partition information memory
void gpt_cleanup(void);

