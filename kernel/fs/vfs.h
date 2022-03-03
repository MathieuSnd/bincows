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
 * @brief open a directory entry
 * 
 * this function shouldn't be used outside the
 * vfs routines: prefer vfs_open_file / vfs_opendir
 * 
 *
 * @param path the directory path
 * @param dir (output) directory entry descriptor
 * @return fs_t NULL if the directory does
 * not exist. the file system associated with the dir
 * otherwise
 */
fs_t *vfs_open(const char *path, fast_dirent_t *dir);


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

    // file associated fs
    fs_t* fs;

    // if fs->cachable = 0: this field is 
    // undefined. Else, writes/reads can
    // rely on the granularity buffer
    unsigned buffer_valid;

    // the filesystems have sector granularity
    // so we put those fields to buffer the accesses
    // and have byte granularity

    // byte offset in the current sector
    // equals fs->file_granularity if the 
    // buffer is empty
    unsigned sector_offset;

    // cursor's sector number
    // should be equal to:
    // (file_offset % fs->file_access_granularity)
    size_t sector_count;

    // current byte offset in the file
    uint64_t file_offset;


    // buffer of fs->file_access_granularity
    // bytes.
    // keep the current sector buffered
    void* sector_buff;


    // VFS private open file structure
    // contains the file handler
    // along with informations about
    // handlers pointing on it
    struct file_ent* open_vfile;
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
 * by vfs_opendir(...)
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

/**
 * @brief update an element's metadata
 * for now, it only consists of the file
 * size
 * 
 * this should only be called from vfs_files.c
 * 
 * 
 * @param path path of the file
 * @param size new file size
 * @return int 0 if success
 */
int vfs_update_metadata(const char* path, uint64_t cluster, size_t file_size);


/////////////////////////////
// file  open / read / write
/////////////////////////////

file_handle_t* vfs_open_file(const char* filename);
void vfs_close_file(file_handle_t* handle);


size_t vfs_read_file(void* ptr, size_t size, size_t nmemb, 
            file_handle_t* stream);

size_t vfs_write_file(const void* ptr, size_t size, size_t nmemb,
            file_handle_t* stream);


// SEEK_SET, SEEK_END, SEEK_CUR are defined in fs.h

// return 0 if successful (see man fseek)
int vfs_seek_file(file_handle_t* stream, uint64_t offset, int whence);
long vfs_tell_file(file_handle_t* stream);

