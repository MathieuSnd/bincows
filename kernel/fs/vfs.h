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


// mount the devfs to /dev
int vfs_mount_devfs(void);


// for vfs_open
#define FS_NO ((fs_t*)0)
/**
 * @brief open a directory entry
 * 
 * this function shouldn't be used outside the
 * vfs routines: prefer vfs_open_file / vfs_opendir
 * 
 *
 * @param path the directory path
 * @param dir (output) directory entry descriptor
 * @return fs_t FS_NO if the directory does
 * not exist. the file system associated with the dir
 * otherwise
 */
fs_t *vfs_open(const char *path, fast_dirent_t *dir);


/**
 * @brief unmount a fs
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
 * @brief create a file.
 * Can only create file if
 * the parent directory already 
 * exists
 * 
 * 
 * @param path file path to create
 * @param type DT_DIR, DT_REG, ...
 * @return 0 on success,
 * non 0 on failure
 */
int vfs_create(const char* path, int type);


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
    // id in the table, contains the file 
    // handler along with informations 
    // about handlers pointing on it
    uint64_t vfile_id;

    // input flags of the open call
    int flags;
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

    // operation rights if the entry
    // represents a file. Else,
    // this field is undefined
    file_rights_t rights;
    
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
 * duplicate a DIR structure
 * the output one should also
 * be closed with vfs_closedir
 */
struct DIR* vfs_dir_dup(struct DIR* dir);


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
int vfs_update_metadata_disk(const char* path, uint64_t cluster, size_t file_size);
int vfs_update_metadata_cache(const char* path, uint64_t cluster, size_t file_size);


/** 
 * @brief truncate a file
 * 
 * @param handle the file handle
 * @param size the new size
 * @return int 0 if success
 */
int vfs_truncate_file(file_handle_t *handle, uint64_t size);

/**
 * @brief flush the file metadata to disk
 * This function is called by the kernel
 * process.
 * 
 * Flushes the file metadata that hasn't been 
 * yet.
 * 
 */
void vfs_lazy_flush(void);

/////////////////////////////
// file  open / read / write
/////////////////////////////

#define VFS_READ 1
#define VFS_WRITE 2
#define VFS_RDWR 3

#define VFS_SEEKABLE 4
#define VFS_TRUNCATABLE 8

#define VFS_APPEND 32

/** 
 * @brief open a file
 * 
 * @param path the file path
 * @param flags 
 * @return file_handle_t* NULL if the file does not exist.
 * Otherwise, the file handler
 */
file_handle_t* vfs_open_file(const char* filename, int flags);

/**
 * @brief create a file handle
 * on the same file, at the same position
 * 
 * @return file_handle_t* 
 */
file_handle_t* vfs_handle_dup(file_handle_t*);

void vfs_close_file(file_handle_t* handle);


/**
 * @brief replaces /////// with /
 *  replaces /a/b/.. with /a
 *
 * the output path does not contain a final '/'
 * and no '.' or '..'
 *
 * @param dst
 * @param src
 */
void simplify_path(char *dst, const char *src);


size_t vfs_read_file(void* ptr, size_t size, 
            file_handle_t* stream);

size_t vfs_write_file(const void* ptr, size_t size, size_t nmemb,
            file_handle_t* stream);


// SEEK_SET, SEEK_END, SEEK_CUR are defined in fs.h

// return the resulting offset if successful (see man lseek)
// -1 if unsucessfull
uint64_t vfs_seek_file(file_handle_t* stream, uint64_t offset, int whence);
long vfs_tell_file(file_handle_t* stream);

