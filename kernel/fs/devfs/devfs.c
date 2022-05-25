#include <stddef.h>
#include <stdint.h>

#include "../../lib/string.h"
#include "../../lib/logging.h"
#include "../../lib/assert.h"
#include "../../lib/panic.h"
#include "../../memory/heap.h"
#include "devfs.h"


typedef struct devfs_file {
    uint64_t id;

    int (*read )(void* arg,       void* buf, size_t begin, size_t count);
    int (*write)(void* arg, const void* buf, size_t begin, size_t count);

    void* arg;

    uint64_t file_size;

    char name[DEVFS_MAX_NAME_LEN];
} devfs_file_t;


struct devfs_priv {
    size_t n_files;
    devfs_file_t* files;
};


/**
 * there is only one devfs.
 * 
 * therefore it is made available
 * for the kernel to map dev files
 * without fetching it.
 * 
 * 
 */
static fs_t* devfs = NULL;




static int read(struct fs* restrict fs, const file_t* restrict fd,
        void* restrict buf, uint64_t begin, size_t n)  {

    
    assert(fs == devfs);
    assert(fs->type == FS_TYPE_DEVFS);

    struct devfs_priv* priv = (void *)(fs + 1);

    for (size_t i = 0; i < priv->n_files; i++) {
        if (priv->files[i].id == fd->addr) {
            return priv->files[i].read(priv->files[i].arg, buf, begin, n);
        }
    }

    return -1;
}


static int write(struct fs* restrict fs, const file_t* restrict fd, 
        const void* restrict buf, uint64_t begin, size_t n) {
    struct devfs_priv* priv = (void *)(fs + 1);


    assert(fs == devfs);
    assert(fs->type == FS_TYPE_DEVFS);

    for (size_t i = 0; i < priv->n_files; i++) {
        if (priv->files[i].id == fd->addr) {
            return priv->files[i].write(priv->files[i].arg, buf, begin, n);
        }
    }

    return -1;
}


static dirent_t* read_dir(struct fs* restrict fs, uint64_t dir_addr, size_t* restrict n) {
    struct devfs_priv* priv = (void *)(fs + 1);

    if(dir_addr != 0) {
        return NULL;
    }


    assert(fs == devfs);
    assert(fs->type == FS_TYPE_DEVFS);

 
    *n = priv->n_files;

    dirent_t* ret = malloc(sizeof(dirent_t) * priv->n_files);

    for (size_t i = 0; i < priv->n_files; i++) {
        strcpy(ret[i].name, priv->files[i].name);
        ret[i].ino       = priv->files[i].id;
        ret[i].file_size = priv->files[i].file_size;
        ret[i].type      = DT_REG;
    }

    return ret;
}


static void free_dirents(dirent_t* dir) {
    free(dir);
}


static int update_dirent(struct fs* fs, uint64_t dir_addr, 
            const char* file_name, uint64_t file_addr, uint64_t file_size) {
    (void)fs;
    (void)dir_addr;
    (void)file_name;
    (void)file_addr;
    (void)file_size;
    
    return 0;
}


static int add_dirent(struct fs* restrict fs, uint64_t dir_addr, const char* name, 
                uint64_t dirent_addr, uint64_t file_size, unsigned type) {
    (void)fs;
    (void)dir_addr;
    (void)name;
    (void)dirent_addr;
    (void)file_size;
    (void)type;

    return -1;
}



static void devfs_unmount(struct fs* fs) {

    assert(fs == devfs);
    assert(fs->type == FS_TYPE_DEVFS);
    
    struct devfs_priv* priv = (void *)(fs + 1);


    if(priv->n_files) {
        free(priv->files);
    }

    free(devfs);

    devfs = NULL;
}



int devfs_map_device(devfs_file_interface_t fi, const char* name) {
    assert(devfs);
    assert(devfs->type == FS_TYPE_DEVFS);


    static uint64_t cur_id;

    // forbidden id value:
    // correspond to the root id
    if(cur_id == 0)
        cur_id = 1;

    struct devfs_priv* priv = (void *)(devfs + 1);

    priv->files = realloc(priv->files, sizeof(struct devfs_file) * (priv->n_files + 1));



    struct devfs_file* file = &priv->files[priv->n_files];

    file->id = cur_id++;
    file->read = fi.read;
    file->write = fi.write;
    file->arg = fi.arg;
    file->file_size = fi.file_size;

    if (strlen(name) >= DEVFS_MAX_NAME_LEN) {
        return -1;
    }

    strncpy(file->name, name, DEVFS_MAX_NAME_LEN);

    priv->n_files++;

    return 0;
}


int devfs_unmap_device(const char* name) {

    assert(devfs);
    assert(devfs->type == FS_TYPE_DEVFS);

    struct devfs_priv* priv = (void *)(devfs + 1);

    for (size_t i = 0; i < priv->n_files; i++) {
        struct devfs_file* file = &priv->files[i];
        if (strcmp(file->name, name) == 0) {
            free(file);
            priv->n_files--;
            priv->files[i] = priv->files[priv->n_files];
            return 0;
        }
    }

    return -1;
}


static 
void truncate_file(struct fs* restrict fs, 
             const file_t*    restrict fd, 
                   size_t            size
) {
    (void)fs;
    (void)fd;
    (void)size;

    panic("truncate not implemented");
}



fs_t* devfs_mount(void) {
    fs_t* fs = malloc(sizeof(fs_t) + sizeof(struct devfs_priv));
    struct devfs_priv* priv = (void*)(fs + 1);


    priv->n_files = 0;
    priv->files = NULL;

    fs->read_only = 0;
    fs->cacheable = 0;
    fs->seekable  = 1;

    fs->type = FS_TYPE_DEVFS;

    fs->file_access_granularity = 1;

    fs->n_open_files = 0;
    fs->truncatable = 0;

    fs->root_addr = 0;


    fs->read_file_sectors  = read;
    fs->write_file_sectors = write;
    fs->read_dir           = read_dir;
    fs->free_dirents       = free_dirents;
    fs->update_dirent      = update_dirent;
    fs->add_dirent         = add_dirent;
    fs->unmount            = devfs_unmount;
    fs->truncate_file      = truncate_file;




    assert(!devfs);

    fs->name = strdup("devfs");

    return devfs = fs;
}