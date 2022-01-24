#include "vfs.h"

#include "../lib/string.h"
#include "../lib/assert.h"
#include "../lib/logging.h"
#include "../lib/dump.h"
#include "../lib/sprintf.h"
#include "../lib/panic.h"

#include "../memory/heap.h"

#include "fat32/fat32.h"

static dirent_t vfs_root;

static int is_absolute(const char *path)
{
    return path[0] == '/';
}

void init_vfs(void)
{
    vfs_root.name[0] = 0;
    vfs_root.type = DT_DIR;
    vfs_root.children = NULL;
    vfs_root.n_children = 0;
    vfs_root.file_size = 0;

    // virtual dir
    vfs_root.fs = NULL;
}

dirent_t *find_child(dirent_t *dir, const char *name)
{
    if (dir->children == NULL && dir->fs)
    {
        // maybe the children are not
        // yet cached
        dir->fs->read_dir(dir->fs, dir);
    }

    for (unsigned i = 0; i < dir->n_children; i++)
    {

        dirent_t *d = &dir->children[i];

        if (!strcmp(d->name, name))
            return d;
    }

    // 404 not found
    return NULL;
}

static dirent_t *add_dir_entry(
    dirent_t *restrict dir,
    unsigned char type,
    const char *name)
{

    unsigned id = dir->n_children;

    dir->n_children++;
    dir->children = realloc(dir->children, dir->n_children * sizeof(dirent_t));

    dir->children[id].children = NULL;
    dir->children[id].cluster = 0;
    dir->children[id].file_size = 0;
    dir->children[id].n_children = 0;
    dir->children[id].children = NULL;
    dir->children[id].type = type;

    strcpy(dir->children[id].name, name);

    return &dir->children[id];
}

dirent_t *vfs_open_dir(const char *path, int create)
{
    assert(is_absolute(path));

    char *buf = malloc(strlen(path));

    strcpy(buf, path);

    char *sub = strtok(buf, "/");

    dirent_t *dir = &vfs_root;
    dirent_t *d = dir;

    while (sub != NULL)
    {
        d = find_child(dir, sub);
        if (!d)
        {
            if (create)
                d = add_dir_entry(dir, DT_DIR, sub);
            else
            {

                free(buf);

                return NULL;
            }
        }

        dir = d;

        sub = strtok(NULL, "/");
    }

    free(buf);

    return d;
}

void log_tree(dirent_t *dir, int level)
{

    for (int i = 0; i < level + 1; i++)
        puts("--");

    printf(" %u - %s\n", dir->type, dir->name);

    if (dir->type != DT_DIR)
        return;
    if (!strcmp(dir->name, ".") || !strcmp(dir->name, ".."))
        return;

    if (dir->type == DT_DIR && !dir->children && dir->fs)
    {
        fat32_read_dir(dir->fs, dir);
    }

    for (unsigned i = 0; i < dir->n_children; i++)
        log_tree(&dir->children[i], level + 1);

    // free(children);
}

int mount(disk_part_t *part, const char *path)
{
    dirent_t *dir = vfs_open_dir(path, 1);

    fs_t *fs = fat32_detect(part, dir);

    if (!fs)
    {
        log_warn("cannot mount %s, unrecognized format", part->name);
        return 0;
    }

    return 1;
}

file_handler_t *vfs_open_file(const char *filename)
{
    dirent_t *dirent = vfs_open_dir(filename, 0);

    // file does not exist or isn't a file
    if (!dirent || dirent->type != DT_REG)
        return NULL;

    fs_t *restrict fs = dirent->fs;

    // virtual file (doesn't make sens)
    if (!fs)
        return NULL;

    // big allocation to allocate the
    // sector buffer too
    file_handler_t *handler = malloc(
        sizeof(file_handler_t) 
        + fs->file_cursor_size
        // size of one sector
        + fs->file_access_granularity);


    handler->file = dirent;
    handler->fs = fs;
    handler->file_offset = 0;
    // empty buffer
    handler->sector_offset = fs->file_access_granularity;
    handler->sector_buff = (void *)handler + 
                sizeof(file_handler_t) + fs->file_cursor_size;

    fs->open_file(dirent, handler->cursor);
    return handler;
}

void vfs_close_file(file_handler_t *handle)
{

    dirent_t *file = handle->file;

    fs_t *fs = file->fs;

    fs->close_file(handle);
}


dirent_t *read_dir(dirent_t *dir)
{
    assert(dir->type == DT_DIR);
    if (dir->children == NULL && dir->fs)
        dir->fs->read_dir(dir->fs, dir);

    return dir->children;
}


void free_dir(dirent_t *entries)
{
    (void)entries;
    // nothing
}


size_t vfs_read_file(void *ptr, size_t size, size_t nmemb,
                     file_handler_t *restrict stream)
{

    void *buf = stream->sector_buff;
    fs_t *fs = stream->fs;
    dirent_t* file = stream->file;

    unsigned granularity = stream->fs->file_access_granularity;


    unsigned max_read = file->file_size - stream->file_offset;

    unsigned bsize = size * nmemb;

    // check nmemb bounds

    if(max_read < bsize) {
        nmemb = max_read / size;
        bsize = size * nmemb;
    }


    while(bsize) {
        unsigned remaining = file->file_size - stream->file_offset;
        
        unsigned n_ready = granularity - stream->sector_offset;

        if(n_ready > remaining)
            n_ready = remaining;


        if(n_ready >= bsize)
        {
            memcpy(
                ptr,
                buf + stream->sector_offset,
                bsize
            );

            ptr += bsize;
            stream->sector_offset += bsize;
            stream->file_offset   += bsize;

            bsize = 0;
        }
        else
        {
            memcpy(
                ptr,
                buf + stream->sector_offset,
                n_ready
            );

            bsize -= n_ready;


            ptr += n_ready;
            stream->file_offset += n_ready;
            stream->sector_offset = 0;

            fs->read_file_sector(
                stream->fs,
                stream->cursor,
                buf);
        }
    }

    return nmemb;
}


size_t vfs_write_file(const void *ptr, size_t size, size_t nmemb,
                      file_handler_t *stream) {
    (void) ptr;
    (void) size;
    (void) nmemb;
    (void) stream;
    panic("vfs_write_file: unimplemented");
    __builtin_unreachable();
}
