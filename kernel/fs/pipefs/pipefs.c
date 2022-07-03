#include "pipefs.h"
#include "../../sync/spinlock.h"
#include "../../lib/string.h"
#include "../../memory/heap.h"

// for pid_t, tid_t
#include "../../sched/thread.h"


typedef struct {
    pid_t pid;
    tid_t tid;
} thread_id_t;


typedef uint32_t id_t;

/**
 * @brief a pipe is a implemented as
 * a circular buffer.
 * 
 */
typedef struct pipefs_file {
    id_t id;
    void* buffer;
    unsigned tail, head;
    uint8_t eof: 1;

    // if the buffer is full,
    // the write will block.
    // if the buffer is empty,
    // the read will block.
    // when a thread is blocked, 
    // it is added to this list
    thread_id_t* waiters;

} pipefs_file_t;


struct pipefs_priv {
    size_t n_files;

    pipefs_file_t* files;

    fast_spinlock_t lock;
};



static fs_t* pipefs = NULL;


static 
id_t create_file(void) {
    static id_t cur_id = 1;

    uint64_t rf = get_rflags();
    _cli();

    struct pipefs_priv* priv = (void *)(pipefs + 1);
    spinlock_acquire(&priv->lock);



    // add a record to the pipefs
    // in the end of the list. The list
    // should remain sorted by id.
    priv->files = realloc(priv->files, sizeof(pipefs_file_t) * (priv->n_files + 1));

    pipefs_file_t* file = &priv->files[priv->n_files++];

    file->id = priv->n_files;
    file->buffer = malloc(PIPE_SIZE);
    file->tail = 0;
    file->head = 0;
    file->waiters = NULL;
    file->eof = 0;

    id_t id = file->id = cur_id++;


    spinlock_release(&priv->lock);
    set_rflags(rf);

    return id;
}


int create_pipe(file_pair_t* ends) {
    assert(ends);
    assert(pipefs);

    //struct pipefs_priv* priv = (void *)(pipefs + 1);


    id_t id = create_file();
    

    *ends = (file_pair_t){
        .in = (file_t){
            .fs = pipefs,
            .addr = id,
            .file_size = ~0llu,
            .rights = (file_rights_t){
                .read = 1,
                .write = 0,
                .truncatable = 0,
                .seekable = 0,
            },
        },
        .out = (file_t){
            .fs = pipefs,
            .addr = id,
            .file_size = ~0llu,
            .rights = (file_rights_t){
                .read  = 0,
                .write = 1,
                .truncatable = 0,
                .seekable = 0,
            },
        },
    };

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


static int read(struct fs* restrict fs, const file_t* restrict fd,
        void* restrict buf, uint64_t begin, size_t n)  {

    assert(fs->type == FS_TYPE_DEVFS);

    struct pipefs_priv* priv = (void *)(fs + 1);
/*
    for (size_t i = 0; i < priv->n_files; i++) {
        if (priv->files[i].id == fd->addr) {
            int rd = 0;
            while(rd || eof) {

            }
        }
    }
*/
    return -1;
}


static int write(struct fs* restrict fs, const file_t* restrict fd, 
        const void* restrict buf, uint64_t begin, size_t n) {
    struct devfs_priv* priv = (void *)(fs + 1);

    assert(fs->type == FS_TYPE_DEVFS);
/*
    for (size_t i = 0; i < priv->n_files; i++) {
        if (priv->files[i].id == fd->addr) {
            return priv->files[i].write(priv->files[i].arg, buf, begin, n);
        }
    }
*/
    return -1;
}


static dirent_t* read_dir(struct fs* restrict fs, uint64_t dir_addr, size_t* restrict n) {
    struct devfs_priv* priv = (void *)(fs + 1);

    panic("unimplemented");
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



static 
void init_priv(struct pipefs_priv* priv) {
    priv->files = NULL;
    priv->n_files = 0;

    spinlock_init((spinlock_t*)&priv->lock);
}


fs_t* pipefs_mount(void) {
    fs_t* fs = malloc(sizeof(fs_t) + sizeof(struct pipefs_priv));

    init_priv((void *)(fs + 1));

    fs->type = FS_TYPE_PIPEFS;
    fs->name = strdup("devfs");
    fs->add_dirent         = add_dirent;
    fs->read_dir           = read_dir;
    fs->free_dirents       = free_dirents;
    fs->update_dirent      = update_dirent;
    fs->read_file_sectors  = read;
    fs->write_file_sectors = write;
    fs->n_open_files = 0;
    fs->part = NULL;
    fs->cacheable = 1;

    fs->root_addr = 0;

    pipefs = fs;

    return 0;
}