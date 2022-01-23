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

    // number of children
    // (directories only)
    unsigned n_children;

    // the associated filesystem 
    // NULL if this is a virtual 
    // file/dir
    struct fs* fs;
    
    // file size in bytes
    // 0 if it is a directory
    uint32_t file_size;

    // Linux complient
    unsigned char type;
    char     name[256];
} dirent_t;


/**
 * @brief generic filesystem interface
 */
typedef struct fs {
    disk_part_t* restrict part;

    // type identifier of the fs
    // ex: FS_TYPE_FAT.
    char type;

    // size of clusters for fat like fs,
    // generally the access granularity
    // of the file
    unsigned file_granularity;


    /**
     * @brief create a cursor over a file
     * 
     * @param file the file to open
     * @param cur (output) the cursor specific handler
     */
    void* (*open_file)(dirent_t* restrict file);

    /**
     * @brief close a cursor
     * 
     * @param cur file specific cursor handler
     */
    void (*close_file)(void *);


    /**
     * @brief read one sector from file
     * the whole buffer will be overwritten even if
     * only one byte is read. Advance the cursor
     * 
     * @param fs partition structure
     * @param cur file cursor specific handler
     * @param buf buffer that is big enough to hold one sector
     * @return int the number of read bytes.
     */
    int (*read_file_sector)(struct fs* restrict fs,void* cur,void* restrict buf);


    /**
     * @brief read one sector from file
     * the whole buffer will be overwritten even if
     * only one byte is read. Advance the cursor
     * 
     * @param fs partition structure
     * @param cur file cursor specific handler
     * @param buf buffer to write from
     * @param size must be 0 if this is't the last 
     * sector in the file. Else, it will be the size 
     * offset of the last sector.
     * @return int the number of read bytes.
     */
    int (*write_file_sector)(struct fs* restrict fs,void* cur, 
            const void* restrict buf,int size);


    // UNIX fseek like 
    int (*seek)(struct fs* restrict fs,void* restrict cur,
            uint64_t offset,int whence);


    /**
     * @brief read a directory and return its
     * children
     * 
     * @param fs filesystem structure
     * @param dir directory to read
     * @param n_entries (output) the number of
     * entries found in the directory
     * @return dirent_t* allocated on heap, 
     * must free later on!
     */
    dirent_t* (*read_dir)(struct fs* fs, dirent_t* dir, int* n_entries);

    dirent_t* root;
} fs_t;


#define FS_TYPE_FAT 1

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

// return non 0 of if did mount
int try_mount(disk_part_t* part);

