#pragma once

#include "../fs.h"

fs_t* fat32_mount(disk_part_t* part);


typedef struct fat32_file_cursor {
    // file's filesystem
    // must be a fat32 one
    fs_t* fs;

    // physical cluster id
    uint32_t cur_cluster;

    // current sector in the 
    // current cluster
    unsigned cur_cluster_offset;

    // byte offset in the file
    unsigned file_offset;

    unsigned file_size;

    // non zero if the cursor reached the
    // end of the file
    int end;

} fat32_file_cursor_t;


/**
 * @brief create a cursor over a file
 * 
 * @param file the file to open
 * @param cur (output) the cursor to that file
 */
void fat32_open_file(file_t* restrict file, fat32_file_cursor_t* cur);

void fat32_close_file(fat32_file_cursor_t *);


/**
 * @brief read one sector from file
 * the whole buffer will be overwritten even if
 * only one byte is read. Advance the cursor
 * 
 * @param fs fat32 partition structure
 * @param cur the file cursor structure
 * @param buf buffer that is big enough to hold one sector
 * @return int the number of read bytes.
 */
int fat32_read_file_sector(
        fs_t* restrict fs, 
        fat32_file_cursor_t* restrict cur, 
        void* restrict buf);


/**
 * @brief read one sector from file
 * the whole buffer will be overwritten even if
 * only one byte is read. Advance the cursor
 * 
 * @param fs fat32 partition structure
 * @param cur the file cursor structure
 * @param buf buffer to write from
 * @param size must be 0 if this is't the last 
 * sector in the file. Else, it will be the size 
 * offset of the last sector.
 * @return int the number of read bytes.
 */
int fat32_write_file_sector(
        fs_t* restrict fs, 
        fat32_file_cursor_t* restrict cur, 
        const void* restrict buf,
        int size);


// UNIX fseek like 
int fat32_seek(
        fs_t* restrict fs, 
        fat32_file_cursor_t* restrict cur,
        uint64_t offset,
        int whence
);


/**
 * @brief read a directory and return its
 * children in a cache structure array
 * 
 * @param fs filesystem structure
 * @param dir directory to read
 * @param n_entries (output) the number of
 * entries found in the directory
 * @return dirent_t* allocated, 
 * must free later on with fat32_free_dirents
 */
dirent_t* fat32_read_dir(fs_t* fs, uint64_t dir_addr, size_t* n);


/**
 * @brief free the output list from fat32_read_dir
 * 
 */
void fat32_free_dirents(dirent_t*);



/**
 * @brief destruct the fs structure,
 * cleanup every allocated memory
 * before being called, the 
 * n_open_files must be cleared.
 * 
 */
void fat32_unmount(fs_t* fs);


