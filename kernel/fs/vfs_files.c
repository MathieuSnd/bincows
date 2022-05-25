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
            uint32_t file_size;
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
    unsigned n_insts;

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


    static uint64_t id = 0;

    open_files[n_open_files - 1] = (struct file_ent) {
        .addr = addr,
        .fs   = fs,
        .file_size = file_len,
        .path = pathbuf,
        .id   = ++id,

        .fhs = NULL,
        .n_insts = 0,
        .modified = 0,
    };

    spinlock_release(&vfile_lock);

    _sti();


    return id;
}



static struct file_ent* aquire_vfile(uint64_t vfile_id) {
    assert(!interrupt_enable());
    spinlock_acquire(&vfile_lock);

    for(unsigned i = 0; i < n_open_files; i++) {
        struct file_ent* e = &open_files[i];
        
        if(e->id == vfile_id) {
            return e;
        }
    }

    panic("vfs_files: file not found");
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
        struct file_ent* file_ent = aquire_vfile(vfile_id);

        // register the handle in the file entry
        file_ent->n_insts++;
        file_ent->fhs = realloc(file_ent->fhs, 
                            file_ent->n_insts * sizeof(file_handle_t *));


        file_ent->fhs[file_ent->n_insts - 1] = handle;

        release_vfile();
    }
    set_rflags(rf);
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
 * created handler
 */
static 
file_handle_t* create_handler(
                    fs_t* fs, 
                    fast_dirent_t* dirent, 
                    const char* path
) {
    
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
        file_handle_t* restrict protected 
) {
    if(!ent->fs->cacheable)
        return;

    for(unsigned i = 0; i < ent->n_insts; i++) {
        if(ent->fhs[i] != protected)
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




file_handle_t* vfs_open_file(const char *path, int flags) {
    fast_dirent_t dirent;

    // this function asserts that interrupts
    // are enabled
    fs_t *restrict fs = vfs_open(path, &dirent);
    
    if (!fs || fs == FS_NO) 
        return NULL;
    // dirent not found or associated to a 
    // virtual directory

    // file does not exist or isn't a file
    if (dirent.type != DT_REG) {
        return NULL;
    }

    // file is not writable
    if (flags & VFS_WRITE && fs->read_only)
        return NULL;

    
    file_handle_t* h = create_handler(fs, &dirent, path);

    if(flags & VFS_APPEND)
        set_stream_offset(h, dirent.file_size);

    h->flags = flags;

    return h;
}


file_handle_t* vfs_handle_dup(file_handle_t* from) {
    // see vfs_open_file

    fs_t* fs = from->fs;



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

    fs->n_open_files++;


    uint64_t vfile_id = from->vfile_id;
    register_handle_to_vfile(vfile_id, new);

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
    
    if(n_lazy_flush_entries > 0) {
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
        struct file_ent* vfile = aquire_vfile(handle->vfile_id);
        flush(vfile);

        release_vfile();
    }

    _sti();
}


/**
 * @todo
 */
void vfs_close_file(file_handle_t *handle) {

    fs_t *fs = handle->fs;

    _cli();
    struct file_ent* restrict open_file = aquire_vfile(handle->vfile_id);


    open_file->n_insts--;

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

    }
    else {
        int found = 0;
        // remove the handle from the list
        for(unsigned i = 0; i < open_file->n_insts + 1; i++) {
            if(handle == open_file->fhs[i]) {
                // found it!
                // now make sure to remove it:
                unsigned to_move = open_file->n_insts - i;

                memmove(
                    &open_file->fhs[i], 
                    &open_file->fhs[i + 1],
                    to_move * sizeof(file_handle_t *)
                );

                // we don't really need to shrink the buffer,
                // it won't ever get very big, and a shrink
                // would happend when the last file handle
                // is closed or a new file handle is opened
                
                found = 1;

                break;
            }
        }
        // the handle is in the file list
        assert(found);
    }

    spinlock_release(&vfile_lock);
    _sti();



    free(handle);
    fs->n_open_files--;


    assert(fs->n_open_files >= 0);
}



// these two functions unsure atomicity of
// file accesses

static void aquire_file_access(uint64_t vfile_id) {
    int accessed = 0;
    
    do {
        _cli();
        {
            struct file_ent* vfile = aquire_vfile(vfile_id);
            accessed = vfile->accessed;
            vfile->accessed = 1;
            release_vfile();
        }
        _sti();

        
        if(accessed)
            sleep(0);

        
        // yield until the process is done
    } while(accessed);
}

static void release_file_access(uint64_t vfile_id) {

    assert(interrupt_enable());

    _cli();
    {
                struct file_ent* vfile = aquire_vfile(vfile_id);
        assert(vfile);
                vfile->accessed = 0;
                release_vfile();
    }
    _sti();
}



int vfs_truncate_file(file_handle_t *handle, uint64_t size)
{
    fs_t *fs = handle->fs;

    assert(interrupt_enable());

    if(     !fs->truncatable 
    // can only truncate files open in write mode
     || (handle->flags & VFS_WRITE) == 0) 
        return -1;


    file_t file;
    // aquire vfile
    {
        _cli();
        struct file_ent* f = aquire_vfile(handle->vfile_id);
        f->accessed = 1;
        file = f->file;
        release_vfile();
        _sti();
    }

    int res = fs->truncate_file(fs, &file, size);

    log_debug("truncate_file: %d", res);

    // release vfile
    {
        _cli();
        struct file_ent* f = aquire_vfile(handle->vfile_id);

        // invalidate access granularity caches for this file
        invalidate_handlers_buffer(f, NULL);
        
        // the operation can fail
        if(!size)
            f->file_size = size;


        f->accessed = 0;
        release_vfile();
        _sti();
    }


    return res;
}




size_t vfs_read_file(void *ptr, size_t size, size_t nmemb,
                     file_handle_t *restrict stream)
{
    assert(interrupt_enable());


    aquire_file_access(stream->vfile_id);

    assert(stream);
    assert(ptr);

    void *const buf = stream->sector_buff;

    fs_t *fs = stream->fs;



    uint64_t file_size;
    {
        _cli();
        struct file_ent* vfile = aquire_vfile(stream->vfile_id);

        file_size = vfile->file_size;
        release_vfile();
        _sti();
    }


    unsigned granularity = fs->file_access_granularity;

    unsigned cachable = fs->cacheable;

    uint64_t max_read = file_size - stream->file_offset;

    unsigned bsize = size * nmemb;

    // check nmemb bounds

    if (max_read < bsize)
    {
        nmemb = max_read / size;
        bsize = size * nmemb;
    }
    

    if(bsize == 0) {
        release_file_access(stream->vfile_id);
        return 0;
    }


    // first read unaligned 
    if(stream->buffer_valid && cachable) {
        // we already have some bytes in memory
        size_t size = MIN(granularity - stream->sector_offset, bsize);
        

        memcpy(ptr, buf + stream->sector_offset, size);




        // advance the cursor
        stream->sector_offset += size;
        stream->file_offset += size;

        assert(stream->sector_offset <= granularity);

        if(stream->sector_offset == granularity) {
            stream->sector_offset = 0;
            // we consumed the cached stuff
            stream->buffer_valid = 0;
            stream->sector_count++;
        }
        
        bsize -= size;
        
        if(!bsize) {
            release_file_access(stream->vfile_id);
            return nmemb;
        }

        ptr += size;

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

    file_t file;
    {
        _cli();
        struct file_ent* vfile = aquire_vfile(stream->vfile_id);
        file = vfile->file;
        release_vfile();
        _sti();
    }


    fs->read_file_sectors(
        fs, 
        &file, 
        read_buf, 
        stream->sector_count,
        read_blocks
    );


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
    
    return nmemb;
}



size_t vfs_write_file(const void *ptr, size_t size, size_t nmemb,
                      file_handle_t *stream) {
    unsigned bsize = size * nmemb;

    // assert that interrupts are enabled
    assert(interrupt_enable());



    if(bsize == 0) {
        return 0;
    }

    aquire_file_access(stream->vfile_id);

    

    assert(stream);
    assert(ptr);

    void *const buf = stream->sector_buff;

    fs_t *fs = stream->fs;

    unsigned granularity = fs->file_access_granularity;
    unsigned cachable = fs->cacheable;
    
    


    // this filed is a copy.
    // if the file size is to be updated, we have to 
    // reaquire the vfile.
    
    file_t file_copy;
    _cli();
    {
        struct file_ent* vfile = aquire_vfile(stream->vfile_id);
        file_copy = vfile->file;

        release_vfile();
    }
    _sti();

    if(stream->flags & VFS_APPEND)
        set_stream_offset(stream, file_copy.file_size);

    unsigned must_write = stream->sector_offset + bsize;

    size_t write_blocks = CEIL_DIV(must_write, granularity);



    // offset of the end of the read buffer
    unsigned end_offset = must_write % granularity;

    unsigned write_buf_size = write_blocks * granularity;

    assert(stream->sector_offset + bsize <= write_buf_size);

    

    // print file_copy
    // if the selected write is perfectly aligned,
    // the we don't need another buffer
    if(stream->sector_offset == 0 && end_offset == 0) {
        fs->write_file_sectors(
            fs,
            &file_copy,
            ptr,
            stream->sector_count,
            write_blocks
        );
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
            || (stream->sector_offset + bsize < granularity
                && stream->file_offset + bsize 
                    < file_copy.file_size)
            ) {
            if(!stream->buffer_valid || !cachable) {
                // must read before writing
                fs->read_file_sectors(
                        fs,
                        &file_copy,
                        write_buf,
                        stream->sector_count,
                        1
                );
            }
            else // buf contains cached data of sc block
            {
                memcpy(write_buf, buf, stream->sector_offset);
                

                int64_t remaning = granularity - (bsize + stream->sector_offset);
                // if remaning > 0, there is stuff in sc that we need to read
                if(remaning > 0)
                    memcpy(
                        write_buf + bsize + stream->sector_offset, 
                        buf       + bsize + stream->sector_offset, 
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
                stream->file_offset + bsize < 
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

        
        assert(stream->sector_offset + bsize <= write_buf_size);
        memcpy(write_buf + stream->sector_offset, ptr, bsize);

        
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
    stream->file_offset += bsize;
    stream->sector_count = stream->file_offset / granularity;
    
    if(file_copy.file_size < stream->file_offset) {
        file_copy.file_size = stream->file_offset;
        // update file size
        // we need to reaquire the vfile
        _cli();
        {

                        struct file_ent *vfile = aquire_vfile(stream->vfile_id);
                        vfile->file_size = stream->file_offset;
            
            invalidate_handlers_buffer(vfile, stream);
            
            release_vfile();
        }
        _sti();
    }


    

    release_file_access(stream->vfile_id);

    return nmemb;
}


uint64_t vfs_seek_file(file_handle_t *restrict stream, uint64_t offset, int whence)
{    
    assert(interrupt_enable());

    if(!stream->fs->seekable)
        return -1;
        
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
        absolute_offset += aquire_vfile(stream->vfile_id)->file_size;
        release_vfile();
        _sti();
        break;
    case SEEK_SET:
        break;
    default:
        // bad whence parameter value
        return -1;
    }

    set_stream_offset(stream, absolute_offset);

    // everything is fine: return 0
    return absolute_offset;
}


long vfs_tell_file(file_handle_t *restrict stream)
{
    return stream->file_offset;
}
