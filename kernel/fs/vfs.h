#pragma once

#include "fs.h"

int mount(disk_part_t* part, const char* path);

void vfs_init(void);


typedef struct file_handler {
    dirent_t* file;
    fs_t* fs;

    // the filesystems have sector granularity
    // so we put those fields to buffer the accesses
    // and have byte granularity

    // byte offset in the current sector
    // equals fs->file_granularity if the 
    // buffer is empty
    unsigned sector_offset;

    // current byte offset in the file
    uint64_t file_offset;

    // buffer of fs->file_access_granularity
    // bytes.
    // keep the current sector buffered
    void* sector_buff;



    // this fild's size will depend
    // be fs->file_cursor_size
    //
    // uint64_t: make sure the structure
    // will be aligned
    uint64_t cursor[0];
} file_handler_t;


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


/**
 * @brief read a directory
 * if the dir->children is NULL and dir
 * doesn't represent a virtual directory,
 * then this function will find its children
 * and fill the structure fields.
 * 
 * @param dir the directory to read
 * @return dirent_t* the new dir->children value
 */
dirent_t* read_dir(dirent_t* dir);


/**
 * @brief free directory entries returned by
 * read_dir()
 * 
 */
void free_dir(dirent_t*);

file_handler_t* vfs_open_file(const char* filename);
void vfs_close_file(file_handler_t* handle);


size_t vfs_read_file(void* ptr, size_t size, size_t nmemb, 
            file_handler_t* stream);

size_t vfs_write_file(const void* ptr, size_t size, size_t nmemb,
            file_handler_t* stream);



#define MAX_PATH_LENGTH 1024

