#pragma once

#include "fs.h"

/**
 * @brief read the partition and
 * add it to the vfs.
 * 
 * @param part the given partitio
 * descriptor
 * @param path the path of the 
 * partition's root directory
 * @return int 0 iif something
 * went wroung and the partition
 * couldn't be mounted
 */
int vfs_mount(disk_part_t* part, const char* path);


/**
 * @brief unmount a partition
 * from the vfs
 * 
 * @param path the path of the 
 * partition's root
 * @return int 0 iif something
 * went wroung and the partition
 * couldn't be unmounted
 */
int vfs_unmount(const char* path);

void vfs_init(void);

// unmount every partition,
// free every memory block
void vfs_cleanup(void);


/**
 * @brief file handler used to read,
 * write, seek. Uses polymorphic calls
 * to underlying fs format.
 * 
 * fields will be redoundent
 * with the information in 'cursor'
 * ex: file_offset, file_size
 * we should be very careful when
 * modifying the structure: always
 * make the underlying fs know
 * what we are doing with this structure
 */
typedef struct file_handler {
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

    // total file size in bytes.
    uint64_t file_size;

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
} file_handle_t;



typedef struct dir_cache_ent {
    // implementation dependant
    uint64_t cluster;

    // file size in bytes
    // 0 if it is a directory
    uint32_t file_size;


    // Linux complient
    unsigned char type;


    // the associated filesystem 
    // NULL if this is a virtual 
    // file/dir
    struct fs* restrict fs;
    
    // null-terminated path name pointer, 
    // max path length = MAX_PATH
    char* path;
} dir_cache_ent_t;



struct DIR {
    unsigned cur;
    unsigned len;
    struct dirent children[0];
};



/////////////////////////////
// dir  open / read / write
/////////////////////////////



/**
 * @brief open a directory entry
 * 
 * @param path the directory path
 * @return struct DIR* NULL if the directory does not exist.
 * Otherwise, the dirent structure 
 * for the file
 */
struct DIR* vfs_opendir(const char* path);

/**
 * @brief close a dir opened
 * by vfs_open_dir(...)
 * 
 */
void vfs_closedir(struct DIR*);


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
struct dirent* vfs_readdir(struct DIR* dir);



/////////////////////////////
// file  open / read / write
/////////////////////////////

file_handle_t* vfs_open_file(const char* filename);
void vfs_close_file(file_handle_t* handle);


size_t vfs_read_file(void* ptr, size_t size, size_t nmemb, 
            file_handle_t* stream);

size_t vfs_write_file(const void* ptr, size_t size, size_t nmemb,
            file_handle_t* stream);
