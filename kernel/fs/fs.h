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



/**
 * file descriptor
 * !! the actual size of this structure
 * is actually sizeof(file_t) + strlen(file.path)+1
 */
typedef struct file {
    // implementation dependant
    uint64_t addr;

    // the associated filesystem 
    // NULL if this is a virtual 
    // file/dir
    struct fs* restrict fs;
    
    // file size in bytes
    // 0 if it is a directory
    uint32_t file_size;
} file_t;



#define MAX_FILENAME_LEN 255
#define MAX_PATH 4095


/**
 * unix complient inode 
 * type used by struct dirent and such
 * 
 */
// value 0 is reserved for virtual
// directories: they are not in the fs but rather
// in the vfs
typedef uint64_t ino_t;

/**
 * @brief almost linux complient dirent
 * (missing d_off)
 * structure used by vfs_read_dir
 */
typedef struct dirent {
    ino_t          ino;       /* Inode number */
    unsigned short reclen;    /* Length of this record */
    unsigned char  type;      /* Type of file; not supported
                                    by all filesystem types */
    char           name[MAX_FILENAME_LEN+1]; /* Null-terminated filename */
} dirent_t;


/**
 * same as above but without
 * the name, for moments
 * when it's not needed
 * 
 */
typedef struct fast_dirent {
    ino_t          ino;       /* Inode number */
    unsigned short reclen;    /* Length of this record */
    unsigned char  type;      /* Type of file; not supported
                                    by all filesystem types */
} fast_dirent_t;


/**
 * @brief generic filesystem interface
 */
typedef struct fs {
    disk_part_t* restrict part;

    // type identifier of the fs
    // ex: FS_TYPE_FAT.
    char type;

    // size of sectors for disk like fs,
    // generally the access granularity
    // of any file
    // this field constraints the size of
    // buffers to grant 1-granularity accesses
    unsigned file_access_granularity;


    /**
     * @brief size of the cursor handler
     * data structure used by open_file,
     * close_file, read_file_sector,
     * write_file_sector, seek
     * 
     */
    unsigned file_cursor_size;


    /**
     * @brief number of currently
     * open file in the file system.
     * to be unmounted, this field
     * must be cleared.
     * 
     * This field is vfs private
     */
    unsigned n_open_files;

    /**
     * @brief create a cursor over a file
     * 
     * @param file the file to open
     * @param cur (output) the cursor specific handler
     * must be at least file_cursor_size big
     */
    void (*open_file)(file_t* restrict file, void* cur);

    /**
     * @brief close a cursor
     * 
     * @param cur file specific cursor handler
     * must be at least file_cursor_size big
     */
    void (*close_file)(void *);


    /**
     * @brief read one sector from file
     * the whole buffer will be overwritten even if
     * only one byte is read. Advance the cursor
     * 
     * @param fs partition structure
     * @param cur file cursor specific handler
     * must be at least file_cursor_size big
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
     * must be at least file_cursor_size big
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
     * children. Complete the input dir data
     * structure
     * 
     * @param fs filesystem structure
     * @param dir_addr implementation specific address
     * of the dir to read
     * @param n the (output) number of entries read
     * @return dirent* may be null if the dir is empty
     * or represents an array of at least (*n) dirent 
     * structure elementns
     * 
     */
    dirent_t* (*read_dir)(struct fs* fs, uint64_t dir_addr, size_t* n);

    /**
     * @brief remove a list of dir entries
     * returned by read_dir(...).
     * This should only free the cached
     * memory, and not affect the partition 
     * state
     * 
     * @param dir the output of read_dir(...)
     */
    void (*free_dirents)(dirent_t* dir);


    /**
     * @brief destruct this structure,
     * cleanup every allocated memory
     * before being called, the 
     * n_open_files must be cleared.
     * 
     */
    void (*unmount)(struct fs* fs);

    /**
     * root cluster/inode/...
     * implementation dependant
     */
    uint64_t root_addr;
} fs_t;


#define FS_TYPE_FAT 1

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

