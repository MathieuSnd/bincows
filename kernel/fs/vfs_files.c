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

    char* path;


    // number of distincs file handlers
    // associated with this file
    unsigned n_insts;

    // list of all file handlers
    // pointing to this file
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
struct file_ent* search_or_insert_file(
                fs_t* fs, 
                uint64_t addr,
                uint64_t file_len,
                const char* path
) {

    for(unsigned i = 0; i < n_open_files; i++) {
        struct file_ent* e = &open_files[i];

        if(e->addr == addr && e->fs == fs)
            return e;
    }
    
    // not found
    // let's add a record
    n_open_files++;
    open_files = realloc(open_files, n_open_files * sizeof(struct file_ent));

    char* pathbuf = malloc(strlen(path) + 1);
    strcpy(pathbuf, path);

    open_files[n_open_files - 1] = (struct file_ent) {
        .addr = addr,
        .fs   = fs,
        .file_size = file_len,
        .path = pathbuf,

        .fhs = NULL,
        .n_insts = 0,
    };

    return &open_files[n_open_files - 1];
}


/**
 * @brief create a file handler and
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
    file_handle_t *handler = malloc(
        sizeof(file_handle_t)
        // size of one sector
        + fs->file_access_granularity);


    handler->fs = fs;
    handler->file_offset = 0;
    // empty buffer
    handler->sector_offset = 0;
    handler->buffer_valid = 0;
    handler->sector_count = 0;

    handler->sector_buff = (void *)handler + sizeof(file_handle_t);


    struct file_ent* file_ent = search_or_insert_file(
                        fs, dirent->ino, dirent->file_size, path);

    handler->open_vfile = file_ent;

    // register the handler in the file entry
    file_ent->n_insts++;
    file_ent->fhs = realloc(file_ent->fhs, 
                        file_ent->n_insts * sizeof(file_handle_t *));

    file_ent->fhs[file_ent->n_insts - 1] = handler;


    fs->n_open_files++;
    
    return handler;
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


file_handle_t *vfs_open_file(const char *path) {
    fast_dirent_t dirent;


    fs_t *restrict fs = vfs_open(path, &dirent);

    if (!fs) // dirent not found
        return NULL;

    // file does not exist or isn't a file
    if (dirent.type != DT_REG) {
        return NULL;
    }

    
    return create_handler(fs, &dirent, path);
}


static void flush(struct file_ent* vfile) {
    vfs_update_metadata(
        vfile->path,
        vfile->addr,
        vfile->file_size
    );
}


void vfs_flush_file(file_handle_t *handle) {
    flush(handle->open_vfile);
}


/**
 * @todo
 */
void vfs_close_file(file_handle_t *handle)
{
    fs_t *fs = handle->fs;

    struct file_ent* restrict open_file 
                = handle->open_vfile;


    open_file->n_insts--;

    flush(open_file);

    if(!open_file->n_insts) {
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


    free(handle);
    fs->n_open_files--;
}


size_t vfs_read_file(void *ptr, size_t size, size_t nmemb,
                     file_handle_t *restrict stream)
{

    assert(stream);
    assert(ptr);

    void *const buf = stream->sector_buff;

    fs_t *fs = stream->fs;
    uint64_t file_size = stream->open_vfile->file_size;

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


    fs->read_file_sectors(
        fs, 
        &stream->open_vfile->file, 
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
                      file_handle_t *stream) {
    unsigned bsize = size * nmemb;


    if(bsize == 0)
        return 0;

    assert(stream);
    assert(ptr);

    void *const buf = stream->sector_buff;

    fs_t *fs = stream->fs;

    unsigned granularity = fs->file_access_granularity;
    unsigned cachable = fs->cacheable;
    
    

    unsigned must_write = stream->sector_offset + bsize;

    size_t write_blocks = CEIL_DIV(must_write, granularity);



    // offset of the end of the read buffer
    unsigned end_offset = must_write % granularity;

    unsigned write_buf_size = write_blocks * granularity;

    // if the selected read is perfectly aligned,
    // the we don't need another buffer
    if(stream->sector_offset == 0 && end_offset == 0) {
        fs->write_file_sectors(
            fs,
            &stream->open_vfile->file,
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
        if(stream->sector_offset != 0
            && stream->file_offset - stream->sector_offset 
                < stream->open_vfile->file_size) {
            if(!stream->buffer_valid || !cachable) {
                
                // must read before writing
                fs->read_file_sectors(
                        fs,
                        &stream->open_vfile->file,
                        write_buf,
                        stream->sector_count,
                        1
                );
            }
            else // buf contains cached data of sc block
                memcpy(write_buf, buf, stream->sector_offset);
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
                        stream->open_vfile->file_size) {
                // the end is both unaligned and in the file.
                // if the end is not aligned and the file is 
                // we need to read the last sector too

                fs->read_file_sectors(
                        fs,
                        &stream->open_vfile->file,
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

        memcpy(write_buf + stream->sector_offset, ptr, bsize);

        fs->write_file_sectors(
            fs,
            &stream->open_vfile->file,
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
    
    // update file size
    stream->open_vfile->file_size = MAX(stream->open_vfile->file_size, stream->file_offset);
    invalidate_handlers_buffer(stream->open_vfile, stream);

    return nmemb;
}


int vfs_seek_file(file_handle_t *restrict stream, uint64_t offset, int whence)
{    
    // real file offset: to be
    // calculated with the whence field
    uint64_t absolute_offset = offset;

    switch (whence)
    {
    case SEEK_CUR:
        absolute_offset += stream->file_offset;
        break;
    case SEEK_END:
        absolute_offset += stream->open_vfile->file_size;
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
    return stream->file_offset;
}
