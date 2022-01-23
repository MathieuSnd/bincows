#pragma once

#include "fs.h"

int mount(disk_part_t* part, const char* path);

void vfs_init(void);

/**
 * @brief open a directory entry
 * 
 * @param path the directory path
 * @param create 0: return NULL if the dir entry or an
 * entry in the path does not exist. non-0: recursively
 * create non existing directories in the path 
 * @return dirent_t* NULL if create = 0 and the directory
 * does not exist. Otherwise, the dirent structure 
 * for the file
 */
dirent_t* vfs_open_dir(const char* path, int create);


void* vfs_open_file(const char* filename);
void vfs_close_file(void* handle);

#define MAX_PATH_LENGTH 1024

