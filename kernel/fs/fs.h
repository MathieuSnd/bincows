#pragma once

#include "gpt.h"

// unknown
#define DT_UNKNOWN	0

// directory 
#define DT_DIR	3

// block device
#define DT_BLK	4

// regular file
#define DT_REG	5

// syb link
#define DT_LNK	6

// socket
#define DT_SOCK	7


typedef struct dirent {
    // implementation dependant
    uint64_t cluster;
    
    // NULL if this is not a directory
    // or if chirdren are not cached
    struct dirent* children;
    
    // file size in bytes
    // 0 if it is a directory
    uint32_t file_size;

    // Linux complient
    unsigned char type;
    char     name[256];
} dirent_t;

typedef struct fs {
    disk_part_t* restrict part;
    char type;

    dirent_t root;
} fs_t;

#define FS_TYPE_FAT 1

// return non 0 of if did mount
int try_mount(disk_part_t* part);



