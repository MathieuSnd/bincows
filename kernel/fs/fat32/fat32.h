#pragma once

#include "../fs.h"

fs_t* fat32_detect(disk_part_t* part);


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
void fat32_open_file(dirent_t* restrict file, fat32_file_cursor_t* restrict cur);



/**
 * @brief read one sector from file
 * the whole buffer will be overwritten even if
 * only one byte is read. Advance the cursor
 * 
 * @param fs fat32 partition structure
 * @param file file descriptor
 * @param offset sector offset
 * @param buf buffer that is big enough to hold one sector
 * @return int the number of read bytes.
 */
int fat32_read_file_sector(
        fs_t* restrict fs, 
        fat32_file_cursor_t* restrict cur, 
        void* restrict buf);
