#include <stddef.h>
#include <stdint.h>

#include "../../lib/string.h"
#include "../../lib/logging.h"
#include "../../lib/assert.h"
#include "../../lib/panic.h"
#include "../../memory/heap.h"
#include "../../lib/registers.h"
#include "memfs.h"

#include "../../sched/shm.h"
#include "../../sched/sched.h"

typedef
struct memfsf {
    shmid_t id;
    char* name;
    int n_instances;
    struct shm_instance** instances;
    
    // 0:    the file was created by a user process
    // non0: the file shouldn't be removed when the file
    //       is closed.
    int kernel;
} memfsf_t;


struct memfs_priv {
    size_t n_files;
    memfsf_t* files;
};


static fs_t* memfs = NULL;


void memfs_register_file(char* name, shmid_t id) {
    assert(memfs);

    struct memfs_priv* pr = (void*)(memfs+1);
    // @todo spinlock ????

    pr->files = realloc(pr->files, sizeof(memfsf_t) * (pr->n_files+1));
    pr->files[pr->n_files++] = (memfsf_t) {
        .id = id,
        .name = name,
        .n_instances = 0,
        .instances = 0,
        .kernel = 1
    };
}


static int read_file(struct memfsf* file,
        void* restrict buf, uint64_t begin, size_t n) {
    pid_t current_pid = sched_current_pid();
    // search for current pid

    struct shm_instance* instance = NULL;


    for(int i = 0; i < file->n_instances; i++) {
        if(file->instances[i]->pid == current_pid) {
            instance = file->instances[i];
            break;
        }
    }

    if(!instance) {
        // not found. share with the process
        instance = shm_open(file->id, current_pid);

        if(!instance) {
            // couldn't map or open
            return -1;
        }
        file->instances = realloc(file->instances, sizeof(struct shm_instance) * file->n_instances);
        file->instances[file->n_instances++] = instance;
    }
    
    // found
    struct mem_desc md = {
        .vaddr = instance->vaddr,
    };
    assert(begin + n <= sizeof(struct mem_desc));

    memcpy(buf, &md - begin, n);

    return n;
}


static int read(struct fs* restrict fs, const file_t* restrict fd,
        void* restrict buf, uint64_t begin, size_t n)  {

    assert(fs->type == FS_TYPE_MEMFS);


    struct memfs_priv* priv = (void *)(fs + 1);

    shmid_t id = (shmid_t)fd->addr;

    for (size_t i = 0; i < priv->n_files; i++) {
        memfsf_t* f = &priv->files[i];
        if (f->id == id) {
            
            assert(interrupt_enable());

            return read_file(f, buf, begin, n);
        }
    }

    panic("unreachable");

    return -1;
}


static int write(struct fs* restrict fs, file_t* restrict fd, 
        const void* restrict buf, uint64_t begin, size_t n) {
    (void) fs;
    (void) fd;
    (void) buf;
    (void) begin;
    (void) n;

    panic("unreachable");
    return -1;
}


static dirent_t* read_dir(struct fs* restrict fs, uint64_t dir_addr, size_t* restrict n) {
    struct memfs_priv* priv = (void *)(fs + 1);

    if(dir_addr != 0) {
        return NULL;
    }


    assert(fs == memfs);
    assert(fs->type == FS_TYPE_MEMFS);

 
    *n = priv->n_files;

    dirent_t* ret = malloc(sizeof(dirent_t) * priv->n_files);

    for (size_t i = 0; i < priv->n_files; i++) {
        strcpy(ret[i].name, priv->files[i].name);
        ret[i].ino       = priv->files[i].id;
        ret[i].file_size = sizeof(struct mem_desc);
        ret[i].type      = DT_REG;
        ret[i].rights    = (file_rights_t){
            .exec  = 0,    
            .read  = 1,
            .write = 0,
            .seekable = 1, 
            .truncatable = 1,
        };
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

    // @todo

    return -1;
}



static void memfs_unmount(struct fs* fs) {

    assert(fs == memfs);
    assert(fs->type == FS_TYPE_MEMFS);
    
    struct memfs_priv* priv = (void *)(fs + 1);


    if(priv->n_files) {
        free(priv->files);
    }

    free(memfs);

    memfs = NULL;
}



static 
int truncate_file(struct fs* restrict fs, 
                   file_t*    restrict fd, 
                   size_t            size
) {
    (void)fs;
    (void)fd;
    (void)size;

    panic("truncate not implemented");
}



fs_t* memfs_mount(void) {
    fs_t* fs = malloc(sizeof(fs_t) + sizeof(struct memfs_priv));
    struct memfs_priv* priv = (void*)(fs + 1);


    priv->n_files = 0;
    priv->files = NULL;

    fs->cacheable = 0;

    fs->type = FS_TYPE_MEMFS;

    fs->file_access_granularity = 1;

    fs->n_open_files = 0;

    fs->root_addr = 0;


    fs->open_file          = NULL;
    fs->close_file         = NULL;
    fs->read_file_sectors  = read;
    fs->write_file_sectors = write;
    fs->read_dir           = read_dir;
    fs->free_dirents       = free_dirents;
    fs->update_dirent      = update_dirent;
    fs->add_dirent         = add_dirent;
    fs->unmount            = memfs_unmount;
    fs->truncate_file      = truncate_file;




    assert(!memfs);

    fs->name = strdup("memfs");
    fs->part = NULL;

    return memfs = fs;
}