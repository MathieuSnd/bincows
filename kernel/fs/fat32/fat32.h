#pragma once

#include "../fs.h"

fs_t* fat32_detect(disk_part_t* part, dirent_t* root);


typedef struct fat32_file_cursor {
    dirent_t* restrict file;

    // physical cluster id
    uint32_t cur_cluster;

    // current sector in the 
    // current cluster
    unsigned cur_cluster_offset;

    // byte offset in the file
    unsigned file_offset;

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
fat32_file_cursor_t* fat32_open_file(dirent_t* restrict file);

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
 * children
 * 
 * @param fs filesystem structure
 * @param dir directory to read
 * @param n_entries (output) the number of
 * entries found in the directory
 * @return dirent_t* allocated on heap, 
 * must free later on!
 */
dirent_t* fat32_read_dir(fs_t* fs, dirent_t* dir, int* n_entries);

