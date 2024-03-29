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



typedef union file_rights {
    struct {
        unsigned read       : 1;
        unsigned write      : 1;
        unsigned exec       : 1;
        unsigned seekable   : 1;
        unsigned truncatable: 1;
    };
    uint8_t value;
} file_rights_t;



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
    uint64_t file_size;

    // file operation rights
    file_rights_t rights;
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
 * same as bellow but without
 * the name, for moments
 * when it's not needed
 * 
 */
typedef struct fast_dirent {
    ino_t         ino;       /* Inode number */
    size_t        file_size; /* Length of this record */
    unsigned char type;      /* Type of file; not supported
                                by all filesystem types */
    file_rights_t rights;    /* rights for the file */
} fast_dirent_t;


/**
 * @brief almost linux complient dirent
 * (missing d_off)
 * structure used by vfs_read_dir
 */
typedef struct dirent {
    union {
        fast_dirent_t fast;
        struct {
            ino_t         ino;       /* Inode number */
            size_t        file_size; /* Length of this record */
            unsigned char type;      /* Type of file; not supported
                                        by all filesystem types */
            file_rights_t rights;    /* right for the file */
        };
    };

    char name[MAX_FILENAME_LEN+1];   /* Null-terminated filename */
} dirent_t;

//static_assert(sizeof(dirent_t) == 0x10a, "dirent_t is not the correct size");


typedef uint64_t handle_id_t;



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

    // fs level file properties
    union {
        uint8_t flags;
        struct {
            //unsigned read_only: 1;
            //unsigned write_only: 1;
            unsigned cacheable: 1;
           // unsigned seekable: 1;
           // unsigned truncatable: 1;
        };
    };


    char* restrict name;


    /**
     * @brief size of the file descriptor
     * data structure used by open_file,
     * close_file, read_file_sector,
     * write_file_sector, seek
     * 
     */
    //unsigned fd_size;


    /**
     * @brief number of currently
     * open file in the file system.
     * to be unmounted, this field
     * must be cleared.
     * 
     * This field is vfs private
     */
    int n_open_files;


    /**
     * root cluster/inode/...
     * implementation dependant
     */
    uint64_t root_addr;


    /**
     * @brief truncate a file to a given size.
     * 
     * @param fs the filesystem
     * @param file the file to truncate. It is not
     * constant: a FS can modify fd->addr.
     * WARNING: modifying fd->* except fd->addr forbidden
     * 
     */
    int (*truncate_file)(struct fs* restrict fs, 
                        file_t* restrict file, size_t size);




    /**
     * @brief read sectors at position begin -> begin + n - 1 
     * from file
     * 
     * no size checking is done: unfinded behavior if
     * (begin + n - 1) * file_access_granularity > fd->file_size
     * 
     * All the checking should be done by the VFS
     * 
     * @param fs partition structure
     * @param fd file descriptor
     * must be at least fd_size big
     * @param buf buffer that is big enough to hold one sector
     * @param begin the position (in blocks) of the
     * first sector to read
     * @return int the number of read sectors.
     */
    int (*read_file_sectors)(struct fs* restrict fs, const file_t* restrict fd,
            void* restrict buf, uint64_t begin, size_t n);


    /**
     * @brief write sectors at position begin -> begin + n - 1 
     * from file
     * 
     * no size checking is done: unfinded behavior if
     * (begin + n - 1) * file_access_granularity > fd->file_size
     * 
     * All the checking should be done by the VFS
     * 
     * @param fs partition structure
     * @param fd file descriptor structure. It is not
     * constant: a FS can modify fd->addr.
     * WARNING: modifying fd->* except fd->addr forbidden
     * @param buf buffer to write from
     * @param begin the position (in blocks) of the
     * first sector to write
     * @param n amount of sectors to be written
     * @return int the number of read bytes.
     */
    int (*write_file_sectors)(struct fs* restrict fs, file_t* restrict fd, 
            const void* restrict buf, uint64_t begin, size_t n);


    /**
     * notify the filesystem that the file with address addr
     * is being opened. The openning is VFS wide,
     * it is only called once if multiple processes open it
     * concurrently.
     * 
     * this function can be NULL, in this case it won't be called
     * 
     */
    void (*open_file)(struct fs* restrict fs, uint64_t addr);


    /**
     * notify the filesystem that the file with address addr
     *  is being closed. The closing is VFS wide,
     * it is only called once if multiple processes open it
     * concurrently.
     * 
     * this function can be NULL, in this case it won't be called
     * 
     */
    void (*close_file)(struct fs* restrict fs, uint64_t addr);



    /**
     * notify the filesystem that a file handle is beeing opened
     * on the specific address. This function is called each
     * time a process opens a file, even if it is already opened
     * in the VFS table, unlike open_file.
     * 
     * If this function returns non0, the vfs_open_file call fails
     * so that one FS can only give access to its files to some
     * processes.
     * 
     * If the file is not yet opened in the VFS table, this 
     * function is called before open_file, and open_file is not
     * called if this function returns non 0.
     * 
     * this function can be NULL, in this case it won't be called
     * and the FS assumes the file can be opened.
     * 
     */
    int (*open_instance)(struct fs* restrict fs, uint64_t addr, handle_id_t hid);

    /**
     * notify the filesystem that the file with address addr
     * is being closed. unlike close_file, this function
     * is called each time a handler is closed and not when
     * no more handler is opened for this file.
     * 
     * this function can be NULL, in this case it won't be called
     * 
     */
    void (*close_instance)(struct fs* restrict fs, uint64_t addr, handle_id_t hid);

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
    dirent_t* (*read_dir)(struct fs* restrict fs, uint64_t dir_addr, size_t* restrict n);

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
     * @brief update the dirent with given 
     * name of a directory which implementation 
     * specific address is given
     * 
     * 
     * @param diraddr implementation specific address
     * 
     * 
     * @return int  0 on success
     */
    int (*update_dirent)(struct fs* fs, uint64_t dir_addr, 
                const char* file_name, uint64_t file_addr, uint64_t file_size);


    /**
     * @brief add a dirent to a given directory
     * 
     * @param dir_addr  the directory to add a dirent to
     * @param name      name of the dirent to create
     * @param dirent_addr the address of the dirent to create
     * @param file_szie must be 0 if the dirent to create is not 
     * a file. Otherwise, it should be the filesize in bytes
     * @param type the type of the dirent to create (DT_REG, DT_DIR, ...)
     * @return int 0 on success
     */
    int (*add_dirent)(struct fs* restrict fs, uint64_t dir_addr, const char* name, 
                    uint64_t* dirent_addr, unsigned type);

/*
    int remove_dirent(uint64_t addr, uint64_t src_parent_addr);
*/




    /**
     * @brief destruct this structure,
     * cleanup every allocated memory
     * before being called, the 
     * n_open_files must be cleared.
     * 
     * The checking n_open_files checking 
     * should be done by the VFS though
     * 
     */
    void (*unmount)(struct fs* fs);

} fs_t;



// fs::type field values
#define FS_TYPE_FAT 1
#define FS_TYPE_DEVFS 2
#define FS_TYPE_PIPEFS 3
#define FS_TYPE_MEMFS 3

// seek whence field values
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

