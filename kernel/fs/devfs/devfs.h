#pragma once

#include "../fs.h"

fs_t* devfs_mount(void);

#define DEVFS_MAX_NAME_LEN 32

typedef struct devfs_file_interface {
    int (*read )(void* arg,       void* buf, size_t begin, size_t count);
    int (*write)(void* arg, const void* buf, size_t begin, size_t count);

    uint64_t file_size;

    // rights.truncatable is forbiden
    file_rights_t rights;

    // arg that will be passed to read/write
    void* arg;
} devfs_file_interface_t;


/**
 * @brief map a file to a device
 * 
 * fi.rights.truncatable is forbiden
 * fi.read won't be called if rights.read is false
 * fi.write won't be called if rights.write is false
 * 
 * return 0 on success, -1 on failure
 */
int devfs_map_device(devfs_file_interface_t fi, const char* name);


/**
 * @brief unmount the devfs dev 
 * 
 * return 0 on success, -1 if the file
 * isn't found
 * 
 */
int devfs_unmap_device(const char* name);

