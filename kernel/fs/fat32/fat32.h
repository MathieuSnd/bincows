#pragma once

#include "../fs.h"

fs_t* fat32_detect(disk_part_t* part);

/**
 * @brief read one sector from file
 * the whole buffer will be overwritten even if
 * only one byte is read
 * 
 * @param fs fat32 partition structure
 * @param file file descriptor
 * @param offset sector offset
 * @param buf buffer that is big enough to hold one sector
 * @return int the number of read bytes.
 */
int fat32_read_file_sector(fs_t* fs, dirent_t* file, int offset, char* buf);

