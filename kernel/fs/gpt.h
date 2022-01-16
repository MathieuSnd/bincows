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

#define PARTITION_TYPE_



void gpt_scan(const struct storage_interface* sti);


// release partition information memory
void gpt_cleanup(void);

