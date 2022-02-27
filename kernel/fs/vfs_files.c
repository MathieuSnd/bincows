#include "vfs.h"

#include "../lib/string.h"
#include "../lib/assert.h"
#include "../lib/logging.h"
#include "../lib/dump.h"
#include "../lib/math.h"
#include "../lib/panic.h"
#include "../acpi/power.h"

#include "../memory/heap.h"

#include "fat32/fat32.h"


/**
 * for now, we use a sequencial 
 * search in the file table to
 * invalid cahces of multiple used 
 * 
 */


struct file_ent {
    file_t file;

    // number of distincs file handlers
    // associated with this file
    unsigned n_insts;

    // 
    file_handle_t** fhs;
};



static unsigned n_open_files = 0;
static struct file_ent* open_files = NULL;


void vfs_files_init(void) {

}

void vfs_files_cleanup(void) {
    // @TODO
    for(unsigned i = 0; i < n_open_files; i++) {
        if(open_files[i].fhs)
            free(open_files[i].fhs);
    }

    if(open_files)
        free(open_files);
}

static 
struct file_ent* search_or_insert_file(fs_t* fs, uint64_t addr, uint64_t file_len) {

    for(unsigned i = 0; i < n_open_files; i++) {
        struct file_ent* e = &open_files[i];

        if(e->file.addr == addr && e->file.fs == fs)
            return e;
    }
    
    // not found
    // let's add a record
    n_open_files++;
    open_files = realloc(open_files, n_open_files * sizeof(struct file_ent));

    open_files[n_open_files - 1] = (struct file_ent) {
        .file = {
            .addr = addr,
            .fs   = fs,
            .file_size = file_len,
        },
        .fhs = NULL,
        .n_insts = 0,
    };

    return &open_files[n_open_files - 1];
}


static 
file_handle_t* create_handler(fs_t* fs, fast_dirent_t* dirent) {
    
    // big allocation to allocate the
    // sector buffer too
    file_handle_t *handler = malloc(
        sizeof(file_handle_t)
        // size of one sector
        + fs->file_access_granularity);


    handler->file_size = dirent->file_size;
    handler->fs = fs;
    handler->file_offset = 0;
    // empty buffer
    handler->sector_offset = 0;
    handler->buffer_valid = 0;
    handler->sector_count = 0;

    handler->sector_buff = (void *)handler + sizeof(file_handle_t);


    struct file_ent* file_ent = search_or_insert_file(
                        fs, dirent->ino, dirent->file_size);

    handler->file = &file_ent->file;

    // register the handler in the file entry
    file_ent->n_insts++;
    file_ent->fhs = realloc(file_ent->fhs, 
                        file_ent->n_insts * sizeof(file_handle_t *));

    
    return handler;
}


file_handle_t *vfs_open_file(const char *path) {
    fast_dirent_t dirent;


    fs_t *restrict fs = vfs_open(path, &dirent);

    if (!fs) // dirent not found
        return NULL;

    // file does not exist or isn't a file
    if (dirent.type != DT_REG)
        return NULL;


    return create_handler(fs, &dirent);
}


void vfs_close_file(file_handle_t *handle)
{
    fs_t *fs = handle->fs;

    fs->n_open_files++;

    free(handle);
}


size_t vfs_read_file(void *ptr, size_t size, size_t nmemb,
                     file_handle_t *restrict stream)
{

    assert(stream);
    assert(ptr);

    void *const buf = stream->sector_buff;

    fs_t *fs = stream->fs;
    uint64_t file_size = stream->file_size;

    unsigned granularity = fs->file_access_granularity;

    unsigned cachable = fs->cacheable;

    unsigned max_read = file_size - stream->file_offset;

    unsigned bsize = size * nmemb;

    // check nmemb bounds

    if (max_read < bsize)
    {
        nmemb = max_read / size;
        bsize = size * nmemb;
    }

    if(bsize == 0)
        return 0;
    

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
        
        if(!bsize)
            return nmemb;

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
    unsigned end_offset = bsize % granularity;

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


    fs->read_file_sectors(
        fs, 
        stream->file, 
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

    
    return nmemb;
}



size_t vfs_write_file(const void *ptr, size_t size, size_t nmemb,
                      file_handle_t *stream)
{
    (void)size;
    assert(stream);
    assert(ptr);
    assert(0);

    return nmemb;
}


int vfs_seek_file(file_handle_t *restrict stream, uint64_t offset, int whence)
{
    (void)stream;
    (void)offset;
    (void)whence;
    
    // real file offset: to be
    // calculated with the whence field
    uint64_t absolute_offset = offset;

    switch (whence)
    {
    case SEEK_CUR:
        absolute_offset += stream->file_offset;
        break;
    case SEEK_END:
        absolute_offset += stream->file_size;
        break;
    case SEEK_SET:
        break;
    default:
        // bad whence parameter value
        return -1;
    }

    size_t block_size = stream->fs->file_access_granularity;


    stream->file_offset   = absolute_offset;
    stream->sector_offset = absolute_offset % block_size;
    stream->sector_count  = absolute_offset / block_size;
    stream->buffer_valid  = 0;

    // everything is fine: return 0
    return 0;
}

long vfs_tell_file(file_handle_t *restrict stream)
{
    return stream->file_size;
}