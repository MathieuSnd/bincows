#include "pipefs.h"
#include "../../sync/spinlock.h"
#include "../../lib/string.h"
#include "../../lib/sprintf.h"
#include "../../lib/panic.h"
#include "../../lib/math.h"
#include "../../memory/heap.h"
#include "../../int/idt.h"

// for pid_t, tid_t
#include "../../sched/thread.h"
// for sched_current_*
#include "../../sched/sched.h"

typedef struct {
    pid_t pid;
    tid_t tid;
} thread_id_t;


#define PIPE_SIZE 1024



typedef uint32_t id_t;


/**
 * @brief a pipe is a implemented as
 * a circular buffer.
 * 
 */
typedef struct pipefs_file {
    id_t id;
    void* buffer;
    volatile unsigned tail, head;

    // 0: no end is closed
    // 1: write end is closed
    // 2: read end is closed
    volatile uint8_t broken;

    // if the buffer is full,
    // the write will block.
    // if the buffer is empty,
    // the read will block.
    // when a thread is blocked, 
    // it is added to this list
    thread_id_t* waiters;
    unsigned n_waiters;

    spinlock_t lock;

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


    if(priv->n_files > 3) {
        log_warn("pipefs: %u simultaneous pipes", priv->n_files + 1);
    }



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
    file->n_waiters = 0;
    file->broken = 0;
    spinlock_init(&file->lock);


    id_t id = file->id = ++cur_id;


    spinlock_release(&priv->lock);
    set_rflags(rf);

    return id;
}

static 
void remove_file(struct pipefs_priv* priv,  pipefs_file_t* file) {

    free(file->buffer);

    if(file->waiters)
        free(file->waiters);


    pipefs_file_t* last = &priv->files[priv->n_files - 1];

    // the file is not the last element
    // in the list.
    if(file != last) {
        // move the last record to the 
        // position of the removed record
        // and shrink the list
        memcpy(
            file, 
            last, 
            sizeof(pipefs_file_t)
        );
    }

    priv->n_files--;
}


int create_pipe(file_handle_pair_t* ends) {
    assert(ends);
    assert(pipefs);


    id_t id = create_file();

    ///////////////////////////
    /// out end of the pipe ///
    ///////////////////////////

    fast_dirent_t dirent = {
        .file_size = -1,
        .type = DT_REG,
        .rights.value = 1, // read only
        .ino = id,
    };

    
    char path[256];

    // @todo forbit mounting pipe on another mount point
    sprintf(path, "/pipe/out/%u", id);


    ends->out = vfs_open_file_from(pipefs, &dirent,  path, VFS_READ);

    assert(ends->out);



    ///////////////////////////
    //// in end of the pipe ///
    ///////////////////////////


    dirent.rights.value = 2; // write only
    dirent.ino |= 1llu << 63;

    sprintf(path, "/pipe/out/%u", id);

    ends->in = vfs_open_file_from(pipefs, &dirent,  path, VFS_WRITE);

    assert(ends->in);


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


// aquires the file and disables interrupts if found
static pipefs_file_t* get_file(struct pipefs_priv* priv, id_t id) {
    uint64_t rf = get_rflags();

    _cli();
    spinlock_acquire(&priv->lock);

    for (size_t i = 0; i < priv->n_files; i++) {
        if (priv->files[i].id == id) {
            pipefs_file_t* file = &priv->files[i];

            spinlock_acquire(&file->lock);
            spinlock_release(&priv->lock);
            
            return &priv->files[i];
        }
    }
    spinlock_release(&priv->lock);

    set_rflags(rf);

    return NULL;
}


// add the common thread to the list of waiters, release the file
static pipefs_file_t* wait_for_buffer(struct pipefs_priv* priv, pipefs_file_t* file) {
    file->waiters = realloc(file->waiters, sizeof(thread_id_t) * (file->n_waiters + 1));

    id_t id = file->id;

    file->waiters[file->n_waiters++] = (thread_id_t) {
        .pid = sched_current_pid(),
        .tid = sched_current_tid(),
    };

    // release file
    spinlock_release(&file->lock);
    _sti();


    // wait for the buffer to be filled
    if(sched_block()) {
        // signal triggered:
        // should return now
        _cli();
        return NULL;
    }

    // may be filled
    // reaquire file
    return get_file(priv, id);
}

// unblock all threads waiting for the buffer and clear the list
static void broadcast_waiters(pipefs_file_t* file) {
    for (unsigned i = 0; i < file->n_waiters; i++) {
        thread_id_t* waiter = &file->waiters[i];
        sched_unblock(waiter->pid, waiter->tid);
    }
    file->n_waiters = 0;
    // don't free the waiters array
}


static int read(struct fs* restrict fs, const file_t* restrict fd,
        void* restrict buf, uint64_t begin, size_t n)  {

    // unseekable file
    (void) begin;

    uint64_t rf = get_rflags();

    assert(fs->type == FS_TYPE_PIPEFS);

    assert(fd->addr >> 63 == 0);

    struct pipefs_priv* priv = (void *)(fs + 1);

    pipefs_file_t* file = get_file(priv, fd->addr);

    if (!file) {
        return -1;
    }


    if(n == 0) {
        return 0;
    }

    unsigned remaining = n;

    while(remaining) {
        if(file->broken) {
            // UNIX semantics:
            // if the pipe is broken,
            // the read will return EOF (0)
            break;
        }

        unsigned batch_rd = remaining;

        int wrap = 0; // 1 if the head should go back to 0
        int block = 0; // block if the buffer is empty


        // buffer capacity limit
        if(file->head <= file->tail && file->head + batch_rd > file->tail) {
            batch_rd = file->tail - file->head;
            block = 1;
        }

        // circular buffer limit
        if(batch_rd > PIPE_SIZE - file->head) {
            batch_rd = PIPE_SIZE - file->head;

            // block and wrap are supposed to be exclusive:
            // if the iteration blocks, the head will reach
            // the tail before the buffer end.
            assert(!block);
            wrap = 1;
        }
        

        memcpy(buf, file->buffer + file->head, batch_rd);

        if(wrap) {
            file->head = 0;
        } else {
            file->head += batch_rd;
        }

        if(batch_rd)
            broadcast_waiters(file);
    

        remaining -= batch_rd;

        if(block) {
            // there is not enough available data in the buffer.
            // if we read something, we should not block, juste signal 
            // the caller that there is not enough data.

            if(n == remaining) {// actually block
                file = wait_for_buffer(priv, file);
                
                if(!file) {
                    // read interrupted
                    break;
                }
            }
            else  {
                break;
            }
        }
    }

    if(file)
        spinlock_release(&file->lock);
    set_rflags(rf);
    
    

    if(n == remaining) {
        // nothing was read, it means that the read was aborted
        // before reading any byte.
        // UNIX returns with the EINTR error
        return -1;
    }

    return n - remaining;
}


static int write(struct fs* restrict fs, file_t* restrict fd, 
        const void* restrict buf, uint64_t begin, size_t n) {
    
    // unseekable file
    (void) begin;

    uint64_t rf = get_rflags();

    
    assert(fs->type == FS_TYPE_PIPEFS);
    assert(fd->addr >> 63 == 1);

    
    struct pipefs_priv* priv = (void *)(fs + 1);
    
    pipefs_file_t* file = get_file(priv, fd->addr);


    
    if (!file) {
        return -1;
    }

    if(n == 0) {
        return 0;
    }
    
    unsigned remaining = n;
    
    while(remaining) {
        if(file->broken) {
            // UNIX semantics:
            // if the pipe is broken,
            // the write will return EOF (0)


            spinlock_release(&file->lock);
            set_rflags(rf);
            return -1;
        }
        
        unsigned batch_wr = remaining;
        
        int wrap = 0; // 1 if the tail should go back to 0
        int block = 0; // block if the buffer is full
        
        
        // buffer capacity limit
        if(file->tail < file->head && file->tail + batch_wr >= file->head) {
            // buffer full condition: file->tail - file->head = 1 
            batch_wr = file->head - file->tail - 1;
            block = 1;
        }
        
        // circular buffer limit
        if(batch_wr > PIPE_SIZE - file->tail) {
            batch_wr = PIPE_SIZE - file->tail;
            
            // block and wrap are supposed to be exclusive:
            // if the iteration blocks, the tail will reach
            // the head before the buffer end.
            assert(!block);
            wrap = 1;
        }
        
        
        memcpy(file->buffer + file->tail, buf, batch_wr);
        
        if(wrap) {
            file->tail = 0;
        } else {
            file->tail += batch_wr;
        }
        
        if(batch_wr)
            broadcast_waiters(file);
        
        
        remaining -= batch_wr;
        
        if(block)
            file = wait_for_buffer(priv, file);
    }
    
    if(file)
        spinlock_release(&file->lock);
    set_rflags(rf);


    if(n - remaining == 0) {
        // nothing was read, it means that the read was aborted
        // before reading any byte.
        // UNIX returns with the EINTR error
        return -1;
    }
    
    return n - remaining;
}


static void close_file(struct fs* restrict fs, uint64_t addr) {
    assert(fs->type == FS_TYPE_PIPEFS);

    uint64_t rf = get_rflags();

    struct pipefs_priv* priv = (void *)(fs + 1);

    // closing one pipe end breaks it
    // closing the other end closes it


    id_t file_id = addr & ~(1llu << 63);
    int in_end = addr >> 63;

    pipefs_file_t* file = get_file(priv, file_id);


    assert(file);




    if(file->broken) {
        if(in_end == 0) // closing the out (read) end, in end already closed
            assert(file->broken == 1); 
        if(in_end == 1) // closing the in end, out end already closed
            assert(file->broken == 2); 

        // already broken: close the file
        remove_file(priv, file);
    }
    else {
        if(in_end == 0) {
            // closing the out (read) end 
            file->broken = 2;
        }
        else {
            // closing the in (write) end
            file->broken = 1;
        }

        // notify the other end
        broadcast_waiters(file);
    }

    spinlock_release(&file->lock);
    set_rflags(rf);
    
}


static dirent_t* read_dir(struct fs* restrict fs, uint64_t dir_addr, size_t* restrict n) {
    struct pipefs_priv* restrict priv = (void *)(fs + 1);

    /**
     * pipefs directory structure:
     * / (0)
     *    in/ (1)
     *       *pipeid* (*pipeid*)
     *    out/ (2)
     *       *pipeid* (*pipeid*)
     * 
     */


    if(dir_addr == 0) {
        *n = 2;

        dirent_t* dirents = malloc(sizeof(dirent_t) * 2);

        strcpy(dirents[0].name, "in");
        dirents[0].ino = 1;
        dirents[0].type = DT_DIR;
        dirents[0].file_size = 0;
        dirents[0].rights.value = 1; // just readable

        strcpy(dirents[1].name, "out");
        dirents[1].ino = 2;
        dirents[1].type = DT_DIR;
        dirents[1].file_size = 0;
        dirents[1].rights.value = 1; // just readables

        return dirents;
    }
    else if(dir_addr > 2) {
        *n = 0;
        return NULL;
    }

    // in/ or out/
    int in = dir_addr == 1;


    uint64_t rf = get_rflags();

    _cli();
    spinlock_acquire(&priv->lock);

    *n = priv->n_files;
    dirent_t* dirents = malloc(sizeof(dirent_t) * priv->n_files);

    for(unsigned i = 0; i < priv->n_files; i++) {
        pipefs_file_t* file = &priv->files[i];
        if(file->broken & (in ? 1 : 2))
            continue;

        sprintf(dirents[i].name, "%u", file->id);
        dirents[i].ino = file->id;
        dirents[i].type = DT_REG;
        dirents[i].file_size = (size_t) -1;
        dirents[i].rights.value = in ? 2 : 1; 
        // just readable (1) or just writable (2)
    }

    

    spinlock_release(&priv->lock);
    set_rflags(rf);

    return dirents;
}


static void free_dirents(dirent_t* dir) {
    free(dir);
}


static int truncate_file(struct fs* fs, file_t* file, size_t size) {
    (void) fs;
    (void) file;
    (void) size;

    // unseekable file
    panic("tried to truncate pipe");
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

    spinlock_init((spinlock_t*)&priv->lock.val);
}


void unmount(struct fs* fs) {
    struct pipefs_priv* priv = (void *)(fs + 1);

    spinlock_acquire(&priv->lock);

    assert(priv->n_files == 0);
    assert(fs == pipefs);

    if(priv->files)
        free(priv->files);
    free(pipefs);
    pipefs = NULL;

}


fs_t* pipefs_mount(void) {
    fs_t* fs = malloc(sizeof(fs_t) + sizeof(struct pipefs_priv));

    struct pipefs_priv* priv = (void *)(fs + 1);
    init_priv((void *)(fs + 1));

    fs->type = FS_TYPE_PIPEFS;
    fs->name = strdup("pipefs");
    fs->add_dirent         = add_dirent;
    fs->read_dir           = read_dir;
    fs->free_dirents       = free_dirents;
    fs->update_dirent      = update_dirent;
    fs->close_file         = close_file;
    fs->open_file          = NULL;
    fs->read_file_sectors  = read;
    fs->write_file_sectors = write;
    fs->truncate_file      = truncate_file;
    fs->unmount            = unmount;
    fs->n_open_files = 0;
    fs->part = NULL;
    fs->cacheable = 0;
    fs->file_access_granularity = 1;

    fs->root_addr = 0;

    pipefs = fs;


    assert(priv->lock.val == 0);
    return fs;
}