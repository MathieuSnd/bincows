#include "vfs.h"

#include "../lib/string.h"
#include "../lib/assert.h"
#include "../lib/logging.h"
#include "../lib/dump.h"
#include "../lib/math.h"
#include "../lib/panic.h"
#include "../acpi/power.h"

#include "../memory/heap.h"
#include "../sync/spinlock.h"
#include "../sched/sched.h"
#include "../lib/time.h"
#include "../int/idt.h"

#include "fat32/fat32.h"


/**
 * for now, we use a sequencial 
 * search in the file table to
 * invalid cahces of multiple used 
 * 
 */

/**
 * this struct is equivalent to a "vnode".
 * 
 * there is one file_ent object per open file 
 * in the system. Two handlers on the same file
 * have one file_ent object.
 * 
 * This way, we can quite easely synchronize
 * file data and metadata across files without
 * constantly accessing disks.
 * 
 * When all file handlers pointing to an entry
 * are closed, file metadata is flushed to the 
 * disk.
 * For instance, the file size is updated in the 
 * vfs cache an on the disk. 
 * 
 */
struct file_ent {
    // fields of file_t
    union {
        file_t file;
        struct {
            // shortcuts for file fields
            uint64_t addr;
            struct fs* restrict fs;
            uint64_t file_size;
        };
    };

    uint64_t id;

    char* path;

    // this field is protected by the
    // vfile table spinlock
    // it is set to 1 when the file is
    // currently being read from or 
    // written to
    //
    // posix files writes and reads are
    // atomic. 
    int accessed;
    


    // if 1, the file has been modified 
    // and then should be flushed to disk
    int modified;

    // number of distincs file handlers
    // associated with this file
    int n_insts;

    // list of all file handlers
    // pointing to this file
    file_handle_t** fhs;
};



static unsigned n_open_files = 0;
static struct file_ent* open_files = NULL;
// spinlock for table open_files
static fast_spinlock_t vfile_lock = {0};


void vfs_files_init(void) {

}

void vfs_files_cleanup(void) {
    // @TODO

    spinlock_acquire(&vfile_lock);
    for(unsigned i = 0; i < n_open_files; i++) {
        if(open_files[i].fhs)
            free(open_files[i].fhs);
    }

    if(open_files)
        free(open_files);

    
    spinlock_release(&vfile_lock);
}




/**
 * @brief search a vfile in the
 * vfile table. If no such entry
 * exists, create it.
 * 
 * @param fs file fs
 * @param addr file physical
 * implementation dependant
 * address
 * @param file_len file len
 * if the vfile doesn't 
 * exist yet
 * @param path file's vfs path, not 
 * necessarily in canonical
 * form
 * @return struct file_ent* vfile
 */
static 
uint64_t search_or_insert_file(
                fs_t* fs, 
                uint64_t addr,
                uint64_t file_len,
                const char* path
) {
    _cli();
    spinlock_acquire(&vfile_lock);
    
    fs->n_open_files++;


    for(unsigned i = 0; i < n_open_files; i++) {
        struct file_ent* e = &open_files[i];

        if(e->addr == addr && e->fs == fs) {
            spinlock_release(&vfile_lock);
            _sti();
            return e->id;
        }
    }
    
    // not found
    // let's add a record
    n_open_files++;


    open_files = realloc(open_files, n_open_files * sizeof(struct file_ent));
    

    char* pathbuf = malloc(strlen(path) + 1);
    strcpy(pathbuf, path);


    uint64_t vfile_id;
    // protected by the lock
    {
        static uint64_t cur_id = 0;
        vfile_id = ++cur_id;
    }

    open_files[n_open_files - 1] = (struct file_ent) {
        .addr = addr,
        .fs   = fs,
        .file_size = file_len,
        .path = pathbuf,
        .id   = vfile_id,

        .fhs = NULL,
        .n_insts = 0,
        .modified = 0,
    };

    spinlock_release(&vfile_lock);

    _sti();


    if(fs->open_file)
        fs->open_file(fs, addr);


    return vfile_id;
}



static struct file_ent* acquire_vfile(uint64_t vfile_id) {
    assert(!interrupt_enable());
    spinlock_acquire(&vfile_lock);

    for(unsigned i = 0; i < n_open_files; i++) {
        struct file_ent* e = &open_files[i];
        
        if(e->id == vfile_id) {
            return e;
        }
    }

    log_debug("acquire_vfile: race condition");
    return NULL;
}

static void release_vfile(void) {
        spinlock_release(&vfile_lock);
}


// this function can be called 
// with enabled or disabled interrupts
static void register_handle_to_vfile(
                uint64_t vfile_id,
                file_handle_t*    handle
) { 
    handle->vfile_id = vfile_id;

    uint64_t rf = get_rflags();
    _cli();

    {
        struct file_ent* file_ent = acquire_vfile(vfile_id);

        // register the handle in the file entry
        file_ent->n_insts++;
        file_ent->fhs = realloc(file_ent->fhs, 
                            file_ent->n_insts * sizeof(file_handle_t *));


        file_ent->fhs[file_ent->n_insts - 1] = handle;

        release_vfile();
    }
    set_rflags(rf);
}


static handle_id_t handle_next_id(void) {
    static _Atomic handle_id_t id;

    return id++;
}


/**
 * @brief create a file handle and
 * registers it in the open file table:
 * if no handler is already open on this
 * vfile, then the function creates a
 * vfile. Else, it adds the handler to
 * the vfile handler list.
 * 
 * 
 * @param fs the fs 
 * @param dirent fast_dirent_t
 * structure describing the 
 * targeted file
 * @param path vfs path to the file
 * it is relevent to modify the
 * file metadata when flushing
 * This argument should be a
 * pointer to newly allocated
 * path buffer. The buffer 
 * will eventually be freed.
 * The path don't have to be in
 * canonical form
 * @return file_handle_t* the
 * created handler or NULL on
 * failure
 */
static 
file_handle_t* create_handler(
                    fs_t* fs, 
                    fast_dirent_t* dirent, 
                    const char* path,
                    int flags
) {
    handle_id_t hid = handle_next_id();

    int shouldnt_open = 0;
    
    if(fs->open_instance) {
        shouldnt_open = fs->open_instance(fs, dirent->ino, hid);
    }

    if(shouldnt_open) {
        return NULL;
    }

    // big allocation to allocate the
    // sector buffer too
    file_handle_t *handle = malloc(
        sizeof(file_handle_t)
        // size of one sector
        + fs->file_access_granularity);


    handle->fs = fs;
    handle->file_offset = 0;
    // empty buffer
    handle->sector_offset = 0;
    handle->buffer_valid = 0;
    handle->sector_count = 0;
    handle->handle_id = hid;
    handle->flags = flags;
    

    handle->sector_buff = (void *)handle + sizeof(file_handle_t);

    uint64_t vfile_id = search_or_insert_file(
                            fs, dirent->ino, dirent->file_size, path);


    register_handle_to_vfile(vfile_id, handle);

    
    return handle;
}


// after a write on a cachable file,
// we must invalidate the cache of
// the other handlers pointing on 
// our file.
// however, we don't want to invalidate
// our own cache.
static 
void invalidate_handlers_buffer(
        struct file_ent* restrict ent,
        file_handle_t* restrict self 
) {
    if(!ent->fs->cacheable)
        return;

    for(int i = 0; i < ent->n_insts; i++) {
        if(ent->fhs[i] != self)
            ent->fhs[i]->buffer_valid = 0;
    }
}

static
void set_stream_offset(file_handle_t* stream, 
                       uint64_t absolute_offset
) {
    size_t block_size = stream->fs->file_access_granularity;


    stream->file_offset   = absolute_offset;
    stream->sector_offset = absolute_offset % block_size;
    stream->sector_count  = absolute_offset / block_size;
    stream->buffer_valid  = 0;
}


file_handle_t* vfs_open_file_from(fs_t* fs, fast_dirent_t* dirent, const char* path, int flags) {

    assert(flags < 2*VFS_TRUNCATABLE);

    // file does not exist or isn't a file
    if (dirent->type != DT_REG) {
        return NULL;
    }

    // file is not writable
    if ((flags & VFS_WRITE) && !dirent->rights.write)
        return NULL;
    
    if ((flags & VFS_READ) && !dirent->rights.read)
        return NULL;


    if(dirent->rights.seekable)
        flags |= VFS_SEEKABLE;
    
    if(dirent->rights.truncatable)
        flags |= VFS_TRUNCATABLE;


    file_handle_t* h = create_handler(fs, dirent, path, flags);

    if(!h) {
        return NULL;
    }

    if(flags & VFS_APPEND)
        set_stream_offset(h, dirent->file_size);



    return h;
}



file_handle_t* vfs_open_file(const char *path, int flags) {
    fast_dirent_t dirent;
    

    // this function asserts that interrupts
    // are enabled
    fs_t *restrict fs = vfs_open(path, &dirent);


    // dirent not found
    if(fs == FS_NO) {
         if(!(flags & VFS_CREATE)) {
            return NULL;
         }
        else {
            // try to create it
            int res = vfs_create(path, DT_REG);

            if(res) // failed
                return NULL;
            
            else {
                fs = vfs_open(path, &dirent);

                // it shouldn't fail (suppose no one removed
                //  the file since its creation)
                // @todo maybe solve this issue with a FS lock
                // or using vfs_create to atomically open the
                // newly created file
                assert(fs);
                assert(fs != FS_NO);
            }
        }
    }


    return vfs_open_file_from(fs, &dirent, path, flags);
}


file_handle_t* vfs_handle_dup(file_handle_t* restrict from) {
    // see vfs_open_file

    uint64_t rf = get_rflags();
    _cli();
    fs_t* fs = from->fs;
    uint64_t vfile_id = from->vfile_id;

    uint64_t addr;
    handle_id_t hid = handle_next_id();
    
    {
        struct file_ent* e = acquire_vfile(vfile_id);
        addr = e->addr;
        release_vfile();
    }



    int allowed;
    
    if(fs->open_instance) {
        allowed = fs->open_instance(fs, addr, hid);
    }
    else
        allowed = 1;

    if(!allowed) {
        // the FS refuses the handle to be
        // created
        _sti();
        return NULL;
    }


    file_handle_t* new = malloc(
        sizeof(file_handle_t)
        // size of one sector
        + fs->file_access_granularity);


    new->fs = fs;
    new->buffer_valid  = 0;
    new->file_offset   = from->file_offset;
    new->sector_count  = from->sector_count;
    new->sector_offset = from->sector_offset;
    new->sector_buff   = new + 1;
    new->handle_id     = hid;
    new->flags         = from->flags;

    // @todo lock FS operation
    fs->n_open_files++;
    


    register_handle_to_vfile(vfile_id, new);

    set_rflags(rf);

    return new;
}

// lazy flush: a kernel process takes care of
// flushing files metadata to disk.
typedef struct flush_args {
    char* path;
    uint64_t addr;
    uint64_t file_size;
} flush_args_t;


unsigned n_lazy_flush_entries = 0;
static flush_args_t* lazy_flush_entries = NULL;



void vfs_lazy_flush(void) {
    // we should be in interrupts disabled state
    // not in an irq handler
    assert(interrupt_enable());

    flush_args_t* local_lazy_flush_entries;
    unsigned n;

    _cli();
    spinlock_acquire(&vfile_lock);
    // shouldn't be able to add lazy flush entries

    n = n_lazy_flush_entries;
    
    if(n > 0) {
        local_lazy_flush_entries = malloc(
            n * sizeof(flush_args_t));
        

        memcpy(local_lazy_flush_entries, lazy_flush_entries,
            n * sizeof(flush_args_t));

        n_lazy_flush_entries = 0;
        
        free(lazy_flush_entries);
        lazy_flush_entries = NULL;
    }

    spinlock_release(&vfile_lock);
    _sti();


    for(unsigned i = 0; i < n; i++) {
        flush_args_t* args = local_lazy_flush_entries + i;


        // flush it
        int res = vfs_update_metadata_disk(args->path, args->addr, args->file_size);
        free(args->path);
        assert(!res);
    }

    if(n > 0)
        free(local_lazy_flush_entries);
}


static void flush(struct file_ent* vfile) {
    assert(!interrupt_enable());

    if(!vfile->modified)
        return;

    lazy_flush_entries = realloc(lazy_flush_entries, 
                            (n_lazy_flush_entries + 1) * sizeof(flush_args_t));
    
    lazy_flush_entries[n_lazy_flush_entries++] = (flush_args_t) {
        .path = strdup(vfile->path),
        .addr = vfile->addr,
        .file_size = vfile->file_size
    };
  
    
    vfs_update_metadata_cache(
        vfile->path,
        vfile->addr,
        vfile->file_size
    );
    
}


void vfs_flush_file(file_handle_t *handle) {

    //if(handle->flags & VFS_WRITE == 0)
    //    return;

    assert(interrupt_enable());

    _cli();
    {
        struct file_ent* vfile = acquire_vfile(handle->vfile_id);
        flush(vfile);

        release_vfile();
    }

    _sti();
}


// these two functions ensure atomicity of
// file accesses
// return non-null on success, -1 on failure
// this function can fail if the file is 
// concurently closed
// copy the file structure. It should only be 
// modified by the file access owner.
static int acquire_file_access(uint64_t vfile_id, file_t* copy) {
    int accessed = 0;

    assert(interrupt_enable());

    assert(copy);

    do {
        _cli();
        {
            struct file_ent* vfile = acquire_vfile(vfile_id);

            if(!vfile) {
                _sti();
                return -1;
            }
            accessed = vfile->accessed;
            *copy = vfile->file;

            vfile->accessed = 1;
            release_vfile();
            
        }
        _sti();

        
        if(accessed) {
            sched_yield();
        }

        
        // yield until the process is done
    } while(accessed);


    return 0;
}

static void release_file_access(uint64_t vfile_id) {

    assert(interrupt_enable());

    _cli();
    {
        struct file_ent* vfile = acquire_vfile(vfile_id);
        assert(vfile->accessed);

        // the file cannot be closed while it is accessed
        assert(vfile);
        vfile->accessed = 0;
        
        release_vfile();
    }
    _sti();
}


// the vfile to update must be acquired
// already by this thread
// the updater will not have his cache invalidated.
// If it is unwanted, it should be set to NULL.
static 
void update_vfile(uint64_t vfile_id, const file_t* file, 
                    file_handle_t* updater) {
    assert(interrupt_enable());
    _cli();
    {
        log_info("update vfile %u", sched_current_pid());
        struct file_ent *vfile = acquire_vfile(vfile_id);

        invalidate_handlers_buffer(vfile, updater);
        
        vfile->file_size = file->file_size;
        vfile->addr      = file->addr;
                
        release_vfile();
    }
    _sti();
}




void vfs_close_file(file_handle_t *handle) {

    fs_t *fs = handle->fs;

    handle_id_t hid = handle->handle_id;

    file_t file;
    //log_warn("%u.%u close.acquire_file_access", sched_current_pid(),sched_current_tid());
    int res = acquire_file_access(handle->vfile_id, &file);
    //log_warn("%u.%u close.acquire_file_access OK", sched_current_pid(),sched_current_tid());

    


    if(res) { // couldn't acquire the file access
        return; // @todo return an error
    }

    _cli();
    struct file_ent* restrict open_file = acquire_vfile(handle->vfile_id);


    if(fs->close_instance)
        fs->close_instance(fs, open_file->addr, hid);

    open_file->n_insts--;


    assert(open_file->n_insts >= 0);

    // when the file is closed, we 
    // should flush it to disk
    if(handle->flags & VFS_WRITE)
        open_file->modified = 1;

    if(!open_file->n_insts) {
        // @todo do not remove the file every time
        // let the kernel process do it

        // no one holds the file anymore
        flush(open_file);

        // free the vfile
        free(open_file->path);
        free(open_file->fhs);

        uint64_t addr = open_file->addr;


        // remove open_file from 
        // the table
        n_open_files--;

        int found = 0;

        for(unsigned i = 0; i < n_open_files + 1; i++) {
            if(&open_files[i] == open_file) {
                // found it!

                // now make sure to remove it:
                unsigned to_move = n_open_files - i;
                
                memmove(
                    &open_files[i], 
                    &open_files[i + 1],
                    to_move * sizeof(struct file_ent)
                );
                found = 1;
                break;
            }
        }
    
        open_files = realloc(open_files, n_open_files * sizeof(struct file_ent));
        assert(found);


        spinlock_release(&vfile_lock);
        _sti();


        if(fs->close_file)
            fs->close_file(fs, addr);
    }
    else {
        spinlock_release(&vfile_lock);
        _sti();
        release_file_access(handle->vfile_id);
    }


    // @todo solve problem here
    free(handle);
    fs->n_open_files--;


    assert(fs->n_open_files >= 0);
}



int vfs_truncate_file(file_handle_t *handle, uint64_t size)
{
    fs_t *fs = handle->fs;

    assert(interrupt_enable());

    if((handle->flags & VFS_TRUNCATABLE) == 0
    // can only truncate files open in write mode
     || (handle->flags & VFS_WRITE) == 0) 
        return -1;


    file_t file;
    // acquire vfile access and fetch file
    int res = acquire_file_access(handle->vfile_id, &file);

    if(res) { // couldn't acquire the file access
        return -1;
    }

    res = fs->truncate_file(fs, &file, size);

    if(!res)
        update_vfile(handle->vfile_id, &file, NULL);

    release_file_access(handle->vfile_id);

    return res;
}




size_t vfs_read_file(void *ptr, size_t size,
                     file_handle_t *restrict stream)
{
    assert(interrupt_enable());


    assert(stream);
    assert((uint64_t)ptr >= 0x1000);

    if(!(stream->flags & VFS_READ)) {
        log_info("MORUDEL");
        return -1;
    }


    void *const buf = stream->sector_buff;

    fs_t *fs = stream->fs;
    unsigned granularity = fs->file_access_granularity;
    unsigned cachable = fs->cacheable;


    file_t file;
    int res = acquire_file_access(stream->vfile_id, &file);
    
    if(res) { // couldn't acquire the file access
        return -1;
    }
    
    uint64_t file_size = file.file_size;


    
    // file_size can be -1 for infinite files
    if(file_size != (uint64_t)-1 && file_size <= stream->file_offset) {
        // EOF
        release_file_access(stream->vfile_id);
        return 0;
    }

    uint64_t max_read = file_size - stream->file_offset;

    unsigned bsize = MIN(size, max_read);
    unsigned retsize = bsize;

    if(bsize <= 0) {
        // nothing to read
        release_file_access(stream->vfile_id);
        return 0;
    }

    // first read unaligned 
    if(stream->buffer_valid && cachable) {
        // we already have some bytes in memory
        size_t unaligned_size = MIN(granularity - stream->sector_offset, bsize);
        

        memcpy(ptr, buf + stream->sector_offset, unaligned_size);




        // advance the cursor
        stream->sector_offset += unaligned_size;
        stream->file_offset += unaligned_size;

        assert(stream->sector_offset <= granularity);

        if(stream->sector_offset == granularity) {
            stream->sector_offset = 0;
            // we consumed the cached stuff
            stream->buffer_valid = 0;
            stream->sector_count++;
        }
        
        bsize -= unaligned_size;
        
        if(!bsize) {
            release_file_access(stream->vfile_id);
        
            assert(retsize == unaligned_size);
            return retsize;
        }

        ptr += unaligned_size;

        // if there is still bytes to read,
        // what we wanted is to manage unaligned bytes
        // so that we are now aligned
        assert(stream->sector_offset == 0);
        assert(stream->file_offset ==  granularity * stream->sector_count);
    }
    
    
    unsigned must_read = stream->sector_offset + bsize;

    size_t read_blocks = CEIL_DIV(must_read, granularity);

    void* read_buf;

    int unaligned = 0;
    

    // offset of the end of the read buffer
    unsigned end_offset = must_read % granularity;


    unsigned read_buf_size = read_blocks * granularity;

    // if the selected read is perfectly aligned,
    // the we don't need another buffer
    if(stream->sector_offset == 0 && end_offset == 0) {
        read_buf = ptr;
    }
    else {
        unaligned = 1;
        read_buf = malloc(read_buf_size);
    }



    int rd = fs->read_file_sectors(
        fs, 
        &file, 
        read_buf, 
        stream->sector_count,
        read_blocks
    );

    if(rd < 0) {
        release_file_access(stream->vfile_id);
        return rd;
    }

    // if we have read less than we wanted,
    // we have reached the end of the file
    // OR
    // the fs we read from did only send us
    // fewer bytes
    if(rd < (int)read_blocks) {
        // reading less than we wanted
        // is only possible on special files,
        // with granularity = 1 (non block fs)
        // (for now, @todo....)
        assert(granularity == 1); 
        
        // no need to update the sector offset

        read_buf_size = rd;
        bsize -= read_blocks - rd;

        retsize = bsize;
    }


    // need copy if 
    if(unaligned) {
        if(cachable && end_offset) {
            // we read more than what we 
            // wanted.
            // let's buffer what we read in
            // case we reread it later on
            memcpy(buf, read_buf + read_buf_size - granularity, granularity);


            stream->buffer_valid = 1;
        }

        memcpy(ptr, read_buf + stream->sector_offset, bsize);
        free(read_buf);
    }


    // values after the operation
    stream->sector_offset = end_offset;
    stream->file_offset += bsize;
    stream->sector_count = stream->file_offset / granularity; 

    release_file_access(stream->vfile_id);
    
    assert(bsize <= file_size);
    
    return retsize;
}



size_t vfs_write_file(const void *ptr, size_t size, 
                      file_handle_t* restrict stream) {
    

    // assert that interrupts are enabled
    assert(interrupt_enable());
    assert(stream);
    assert(stream->flags < 2*VFS_TRUNCATABLE);

    if(!(stream->flags & VFS_WRITE)) {
        return -1;
    }

    if(size == 0) {
        return 0;
    }

    
    assert(stream);
    assert((uint64_t)ptr >= 0x1000);

    void *const buf = stream->sector_buff;

    fs_t *fs = stream->fs;

    unsigned granularity = fs->file_access_granularity;
    unsigned cachable = fs->cacheable;
    
    


    // this filed is a copy.
    // if the file size is to be updated, we have to 
    // reacquire the vfile.    
    file_t file_copy;

    int res = acquire_file_access(stream->vfile_id, &file_copy);

    if(res) { // couldn't acquire the file access
        return -1;
    }


    if(stream->flags & VFS_APPEND) {
        set_stream_offset(stream, file_copy.file_size);
    }

    unsigned must_write = stream->sector_offset + size;

    size_t write_blocks = CEIL_DIV(must_write, granularity);



    // offset of the end of the read buffer
    size_t end_offset = must_write % granularity;

    size_t write_buf_size = write_blocks * granularity;

    assert(stream->sector_offset + size <= write_buf_size);

    
    // save file address in case it is updated
    uint64_t file_addr = file_copy.addr;

    int error = 0;
    
    // print file_copy
    // if the selected write is perfectly aligned,
    // the we don't need another buffer
    if(stream->sector_offset == 0 && end_offset == 0) {
        int w = fs->write_file_sectors(
            fs,
            &file_copy,
            ptr,
            stream->sector_count,
            write_blocks
        );

        if(w < 0) 
            error = 1;
    }
    /*     
        sc = stream->sector_count

          storage blocks (for n = 5):
        |---------|---------|---------|---------|---------|
        |    sc   |   sc+1  |   sc+2  |   sc+3  |   sc+4  |
        |---------|---------|---------|---------|---------|

        we want to write:
               
        |------===|=========|=========|=========|==-------|
        |   sc |  |   sc+1  |   sc+2  |   sc+3  | | sc+4  |
        |------===|=========|=========|=========|==-------|
        <----->                                 <->
        stream->sector_offset                   end_offset


        to do so, we have to first read sc and 
        sc + n - 1 blocks.
        
        if fs->cachable = 1 and stream->buffer_valid,
        then we don't need to read sc block

        we still have to read sc+4 though.

        We can then put sc+4 in the cache buffer



        we set write_buf:

        |=========|=========|=========|=========|=========|
        |    sc   |   sc+1  |   sc+2  |   sc+3  |   sc+4  |
        |=========|=========|=========|=========|=========|
    */
    else {

        void* write_buf = malloc(write_buf_size);

        // unaligned beginning
        // if the beginning is not beyond
        // file end, we need to read
        // the sector
        if((stream->sector_offset != 0
            && stream->file_offset - stream->sector_offset 
                < file_copy.file_size)

                // less than 1 sector is written
                // and the write doesn't reach 
                // the end of the file
            || (stream->sector_offset + size < granularity
                && stream->file_offset + size 
                    < file_copy.file_size)
            ) {
            if(!stream->buffer_valid || !cachable) {
                // must read before writing
                int rd = fs->read_file_sectors(
                        fs,
                        &file_copy,
                        write_buf,
                        stream->sector_count,
                        1
                );

                if(rd != 1) {
                    // couldn't read enough data.

                    release_file_access(stream->vfile_id);

                    return -1;
                }
            }
            else // buf contains cached data of sc block
            {
                memcpy(write_buf, buf, stream->sector_offset);
                

                int64_t remaning = granularity - (size + stream->sector_offset);
                // if remaning > 0, there is stuff in sc that we need to read
                if(remaning > 0)
                    memcpy(
                        write_buf + size + stream->sector_offset, 
                        buf       + size + stream->sector_offset, 
                        remaning
                    );
            }
        }
        
        uint64_t last_sector = stream->sector_count + write_blocks - 1;

        if(last_sector != stream->sector_count) {

            // we read at least 2 sectors:
            // the granularity buffer is no longer
            // valid
            if(cachable)
                stream->buffer_valid = 0;
            
            if(end_offset != 0 && 
                stream->file_offset + size < 
                        file_copy.file_size) {
                // the end is both unaligned and in the file.
                // if the end is not aligned and the file is 
                // we need to read the last sector too

                fs->read_file_sectors(
                        fs,
                        &file_copy,
                        write_buf + write_buf_size - granularity,
                        last_sector,
                        1
                );

                // we might need that later...
                if(cachable) {
                    stream->buffer_valid = 1;
                    memcpy(
                        buf, 
                        write_buf + write_buf_size - granularity,
                        granularity
                    );
                }

            }
        }

        
        assert(stream->sector_offset + size <= write_buf_size);
        memcpy(write_buf + stream->sector_offset, ptr, size);

        
        fs->write_file_sectors(
            fs,
            &file_copy,
            write_buf,
            stream->sector_count,
            write_blocks
        );
        

        free(write_buf);
    }
    

    // advance the cursor
    stream->sector_offset = end_offset;
    stream->file_offset += size;
    stream->sector_count = stream->file_offset / granularity;


    int size_update = 0;
    int address_update = file_addr != file_copy.addr;
    if(file_copy.file_size != ~0llu 
    && file_copy.file_size < stream->file_offset) {
        size_update = 1;
        file_copy.file_size = stream->file_offset;
    }
    
    
    if(size_update || address_update) 
        // update file size and address
        update_vfile(stream->vfile_id, &file_copy, stream);
    

    release_file_access(stream->vfile_id);

    if(error)
        return -1;

    return size;
}


uint64_t vfs_seek_file(file_handle_t *restrict stream, uint64_t offset, int whence)
{
    assert(interrupt_enable());

    if((stream->flags & VFS_SEEKABLE) == 0) {
        return -1;
    }
        
    // real file offset: to be
    // calculated with the whence field
    uint64_t absolute_offset = offset;

    switch (whence)
    {
    case SEEK_CUR:
        absolute_offset += stream->file_offset;
        break;
    case SEEK_END:
        _cli();
        absolute_offset += acquire_vfile(stream->vfile_id)->file_size;
        release_vfile();
        _sti();
        break;
    case SEEK_SET:
        break;
    default:
        // bad whence parameter value
        return -1;
    }

    _cli();
    acquire_vfile(stream->vfile_id);
    // stream fields are protected by the vfile lock
    set_stream_offset(stream, absolute_offset);
    release_vfile();
    _sti();

    // everything is fine: return the absolute offset
    return absolute_offset;
}


long vfs_tell_file(file_handle_t *restrict stream)
{
    // @todo acquire vfile to make this function atomic
    return stream->file_offset;
}
