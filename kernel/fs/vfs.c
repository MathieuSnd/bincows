#include "vfs.h"

#include "../lib/string.h"
#include "../lib/assert.h"
#include "../lib/logging.h"
#include "../lib/dump.h"
#include "../lib/sprintf.h"
#include "../lib/panic.h"
#include "../lib/math.h"
#include "../acpi/power.h"

#include "../memory/heap.h"

#include "fat32/fat32.h"

typedef struct vdir
{
    // vdir children
    // null if no children
    // or if this is a fs
    // root
    struct vdir *children;
    unsigned n_children;

    // null if this is not a
    // fs root
    fs_t *fs;

    char *path;
} vdir_t;

static vdir_t vfs_root;

// list of every cached dir
// force it to take a bounded
// amount of memory
static dir_cache_ent_t *cached_dir_list;

// cache size must be a power of two
static unsigned cache_size = 0;

// dynamic array of every mounted
// file system
// static fs_t** mounted_fs_list = NULL;
// static unsigned n_mounted_fs = 0;

static int is_absolute(const char *path)
{
    return path[0] == '/';
}

/**
 * @brief replaces /////// with /
 *  replaces /a/b/.. with /a
 *
 * the output path does not contain a final '/'
 * and no '.' or '..'
 *
 * @param dst
 * @param src
 */
static void simplify_path(char *dst, const char *src)
{

    char *buf = malloc(strlen(src) + 1);

    *dst = 0;

    strcpy(buf, src);

    char *sub = strtok(buf, "/");

    // each loop concatenates
    // "/str"
    // so the final output path is in format:
    // "/foo/bar/foobar"
    while (sub != NULL)
    {
        if (!strcmp(sub, "."))
            ;
        else if (!strcmp(sub, ".."))
        {
            char *ptr = (char *)strrchr(dst, '/');

            if (ptr != NULL)
                *ptr = '\0';
        }
        else
        {
            strcat(dst, "/");
            strcat(dst, sub);
        }

        sub = strtok(NULL, "/");
    }

    free(buf);

    if (dst[0] == '\0')
    {
        *dst++ = '/';
        *dst = '\0';
    }
}

/**
 *  hash 0 -> 8191
 */
uint16_t path_hash(const char *path)
{
    unsigned len = strlen(path);
    uint16_t res = 0;
    const uint8_t *dwp = (const uint8_t *)path;

    for (unsigned i = 0; i < len; i += 1)
    {
        uint32_t entropy_bits = dwp[2 * i] ^ (dwp[2 * i + 1] << 4);

        entropy_bits ^= (entropy_bits << 4);

        res = (res ^ (entropy_bits + i)) & 8191;
    }
    return res;
}

// recursively destruct the entire
// vdir tree
static void free_vtree(vdir_t *root)
{
    for (unsigned i = 0; i < root->n_children; i++)
    {
        vdir_t *child = &root->children[i];

        // unmount mounted entries
        if (child->fs)
            child->fs->unmount(child->fs);
        else
            free_vtree(child);

        free(child->path);
        free(child);
    }
    root->n_children = 0;
}

// return the vdir associated with given fs
// or null if it is not found
static vdir_t *search_fs(vdir_t *root, fs_t *fs)
{
    if (root->fs == fs)
        return root;

    for (unsigned i = 0; i < root->n_children; i++)
    {
        vdir_t *child = &root->children[i];

        vdir_t *sub_result = search_fs(child, fs);
        if (sub_result)
            return sub_result;
    }
    return NULL;
}

// remove the vdir
// return wether or not the vdir has
// been removed
// return 0 iif something went wrong
static int remove_vdir(vdir_t *vdir)
{
    // cannot remove the root
    if (vdir == &vfs_root)
        return 0;

    vdir_t *vd = &vfs_root;

    while (1)
    {
        vdir_t *child = NULL;

        for (unsigned i = 0; i < vd->n_children; i++)
        {
            const char *p = vd->children[i].path;

            if (&vd->children[i] == vdir)
            {
                // actually remove it
                memmove(
                    vd->children + i,
                    vd->children + i + 1,
                    vd->n_children - i - 1);

                vd->n_children--;

                return 1;
            }

            // path begins with p,
            // it corresponds!
            if (!strncmp(vdir->path, p, strlen(p)))
            {
                child = &vd->children[i];
                break;
            }
        }

        if (!child)
        {
            return 0;
        }
        else
        {
            vd = child;
        }
    }
}

// return 0 iif something
// went wrong
static int unmount(vdir_t *vdir)
{
    fs_t *fs = vdir->fs;

    assert(vdir);

    if (vdir->n_children)
    {
        log_warn("cannot unmount partition %s: it is not a leaf partition", fs->part->name);

        return 0;
    }

    if (fs->n_open_files != 0)
    {
        log_warn(
            "cannot unmount partition %s: %u open files",
            fs->n_open_files, fs->part->name);

        return 0;
    }

    if (vdir == &vfs_root)
        vfs_root.fs = NULL;
    else
        assert(remove_vdir(vdir));

    // remove cache entries related with this thing
    for (unsigned i = 0; i < cache_size; i++)
    {
        if (cached_dir_list[i].fs == fs)
        {
            free(cached_dir_list[i].path);
            cached_dir_list[i].path = NULL;
        }
    }

    fs->unmount(fs);

    return 1;
}

static void free_cache(void)
{
    for (unsigned i = 0; i < cache_size; i++)
    {

        // this is null if the entry
        // is empty
        if (cached_dir_list[i].path)
        {
            free(cached_dir_list[i].path);
        }
    }
    free(cached_dir_list);
    cache_size = 0;
}

void vfs_init(void)
{
    // cache size must be a power of two
    cache_size = 4096;
    cached_dir_list = malloc(sizeof(dir_cache_ent_t) * cache_size);

    // zero it because it contains name pointers
    memset(cached_dir_list, 0, sizeof(dir_cache_ent_t) * cache_size);

    vfs_root = (vdir_t){
        .children = NULL,
        .n_children = 0,
        .fs = 0,
        .path = "/",
    };

    atshutdown(vfs_cleanup);
}

void vfs_cleanup(void)
{
    free_cache();
    free_vtree(&vfs_root);
}

// return 0 iif entry not present
static int is_cache_entry_present(dir_cache_ent_t *cache_ent)
{
    return cache_ent->path != NULL;
}

static void free_cache_entry(dir_cache_ent_t *cache_ent)
{
    free(cache_ent->path);

    // nothing else to unallocate
}

/**
 * @brief do not forget to call
 * fs->free_dirents(entries)
 * afterwards
 *
 * @param fs the fs associated with the dir
 * @param ino inode number
 * @param dir_path vfs path in canonical form ('/a/b/c')
 * @param n (ouput) array size
 * @return dirent_t* array
 */
static dirent_t *read_dir(fs_t *fs, ino_t ino, const char *dir_path, size_t *n)
{
    assert(cache_size);

    dirent_t *ents = fs->read_dir(fs, ino, n);
    unsigned _n = *n;

    // add cache entries
    for (unsigned i = 0; i < _n; i++)
    {
        dirent_t *dent = &ents[i];

        int len = strlen(dent->name) + strlen(dir_path) + 1;
        assert(len < MAX_PATH);
        char *path = malloc(len + 1);
        strcpy(path, dir_path);
        strcat(path, "/");
        strcat(path, dent->name);

        uint16_t hash = path_hash(path) & (cache_size - 1);

        dir_cache_ent_t *cache_ent = &cached_dir_list[hash];

        if (is_cache_entry_present(cache_ent))
            free_cache_entry(cache_ent);

        cache_ent->cluster = dent->ino;
        cache_ent->file_size = dent->reclen;
        cache_ent->type = dent->type;
        cache_ent->fs = fs;
        cache_ent->path = path;
    }

    return ents;
}

/**
 * @brief return the virtual directory
 * representing the fs on which the file/dir
 * should be or the virtual dir if the path
 * represents a vdir
 *
 * @param path file/dir path in cannonical form
 * @return NULL if no fs is succeptible to
 * contain the path
 */
static vdir_t *get_fs_vdir(const char *path)
{

    vdir_t *vd = &vfs_root;

    // go deeper and deeper in the vfs tree
    // until there is no match
    while (1)
    {
        vdir_t *child = NULL;

        for (unsigned i = 0; i < vd->n_children; i++)
        {
            const char *p = vd->children[i].path;

            // path begins with p,
            // it corresponds!
            if (!strncmp(path, p, strlen(p)))
            {
                child = &vd->children[i];
                break;
            }
        }

        if (!child)
        {
            if (vd->fs == NULL)
                return NULL;

            return vd;
        }
        else
        {
            vd = child;
        }
    }
}

// return the number
// of naste in path
// path in cannonical form
static unsigned nasted_order(const char *path)
{
    unsigned n = 0;

    assert(is_absolute(path));

    // path == '/'
    if (!path[1])
        return 0;

    path--;
    while ((path = strchr(++path, '/')))
        n++;

    return n;
}

/**
 * @brief insert a virtual directory in
 * the vdir tree. The input structure
 * is copied to the tree
 *
 * @param new
 * @return int 0 if it cannot be added
 */
static vdir_t *insert_vdir(vdir_t *new)
{
    vdir_t *vdir = &vfs_root;

    while (1)
    {
        vdir_t *match = NULL;

        for (unsigned i = 0; i < vdir->n_children; i++)
        {
            vdir_t *child = &vdir->children[i];
            if (!strncmp(child->path, new->path, strlen(child->path)))
            {
                match = child;
                break;
            }
        }
        if (match)
            vdir = match;
        else
        {
            // something is already mounted there!
            if (!strcmp(vdir->path, new->path))
                return 0;

            vdir->n_children++;

            vdir->children = realloc(vdir->children, vdir->n_children * sizeof(vdir_t));

            vdir_t *newdir = vdir->children + vdir->n_children - 1;
            memcpy(newdir, new, sizeof(vdir_t));

            return newdir;
        }
    }
}

/**
 * @brief return a
 *
 * @param n the number of vchildren or -1 if the
 * dir does not seem to exist. Warning: the path
 * may exist anyway, it is just doesn't appear
 * in the vfs structure.
 * @param path path in cannonical form
 * @return vdir_t** heap allocated list of virtual
 * children of the path associated dir entry
 */
static vdir_t **get_vchildren(const char *path, int *n)
{

    vdir_t **ret = NULL;

    *n = 0;

    vdir_t *vd = &vfs_root;

    unsigned path_nasted_order = nasted_order(path);

    char exists = 0;

    // go deeper and deeper in the vfs tree
    // until there is no ancestor match

    // /a/b/d/xr
    // /c
    // /a
    //   /a/c/f/z
    //   /a/d/f/z
    //   /a/b
    //     /a/b/d
    //       /a/b/d
    //       /a/b/d/xr/gg
    // /b
    while (1)
    {
        vdir_t *ancestor = NULL;

        for (unsigned i = 0; i < vd->n_children; i++)
        {
            const char *p = vd->children[i].path;

            // it corresponds!
            if (!strncmp(path, p, strlen(path)))
            {
                unsigned no = nasted_order(p);

                // p begins with path,
                // therefore the directory
                // does exist
                exists = 1;

                if (path_nasted_order + 1 == no)
                {
                    // we are in the right directory!
                    unsigned id = (*n)++;
                    ret = realloc(ret, *n * sizeof(vdir_t *));
                    ret[id] = &vd->children[i];
                }
            }
            if (!strncmp(path, p, strlen(p)))
            {
                ancestor = &vd->children[i];
                break;
            }
        }

        if (ancestor)
            vd = ancestor;
        else
        {
            if (!exists)
                *n = -1;
            return ret;
        }
    }
}

/**
 * find a child with a given name
 *
 * @param parent
 * @param child
 * @param name
 * @return int 0 if no child have this
 * name
 */
static int find_fs_child(
    fs_t *restrict fs,
    fast_dirent_t *parent,
    fast_dirent_t *child,
    const char *name)
{
    size_t n;

    int found = 0;

    // no cache
    dirent_t *ents = fs->read_dir(fs, parent->ino, &n);

    for (unsigned i = 0; i < n; i++)
    {
        if (!strcmp(ents[i].name, name))
        {

            child->ino = ents[i].ino;
            child->reclen = ents[i].reclen;
            child->type = ents[i].type;

            found = 1;
            break;
        }
    }

    fs->free_dirents(ents);

    return found;
}

// path in cannonical form
// return NULL if empty / conflict
static dir_cache_ent_t *get_cache_entry(const char *path)
{
    assert(is_absolute(path));

    assert(cache_size);

    uint16_t hash = path_hash(path) & (cache_size - 1);
    // check cache
    dir_cache_ent_t *ent = &cached_dir_list[hash];
    if (ent->path == NULL) // not present entry
        return NULL;

    if (!strcmp(ent->path, path)) // right entry (no collision)
        return ent;

    return NULL;
}

/**
 * @brief open a directory entry
 *
 * @param path the directory path in cannonical
 * form
 * @param dir (output) directory entry descriptor
 * @return fs_t NULL if the directory does
 * not exist. the file system associated with the dir
 * otherwise
 */
static fs_t *open_dir(const char *path, fast_dirent_t *dir)
{
    assert(is_absolute(path));

    // check cache
    dir_cache_ent_t *ent = get_cache_entry(path);
    if (ent)
    { // present entry
        dir->ino = ent->cluster;
        dir->reclen = ent->file_size;
        dir->type = ent->type;

        return ent->fs;
    }

    vdir_t *vdir = get_fs_vdir(path);

    if (!vdir)
    {
        // there is no wat the dir could
        // exist
        return NULL;
    }

    fs_t *fs = vdir->fs;

    char *buf = malloc(strlen(path) + 1);

    strcpy(buf, path + strlen(vdir->path));
    // only keep the FS part
    // of the path

    char *sub = strtok(buf, "/");

    fast_dirent_t cur = {
        .ino = fs->root_addr,
        .reclen = 0,
        .type = DT_DIR,
    };

    while (sub != NULL && *sub)
    {
        fast_dirent_t child;

        if (!find_fs_child(fs, &cur, &child, sub))
        {
            // the child does not exist
            free(buf);

            return NULL;
        }

        cur.ino = child.ino;
        cur.reclen = child.reclen;
        cur.type = child.type;

        sub = strtok(NULL, "/");
    }

    free(buf);

    dir->ino = cur.ino;
    dir->reclen = cur.reclen;
    dir->type = cur.type;

    return fs;
}

static void log_tree(const char *path, int level)
{

    struct DIR *dir = vfs_opendir(path);

    struct dirent *dirent;

    assert(dir);

    while ((dirent = vfs_readdir(dir)))
    {
        for (int i = 0; i < level + 1; i++)
            puts("-");

        printf(" %s (%u)\n", dirent->name, dirent->type);

        if (dirent->type == DT_DIR)
        {
            char *pathb = malloc(1024);

            strcpy(pathb, path);
            strcat(pathb, "/");
            strcat(pathb, dirent->name);

            log_tree(pathb, level + 1);

            free(pathb);
        }
    }

    vfs_closedir(dir);
}

int vfs_mount(disk_part_t *part, const char *path)
{
    // register the new virtual directory
    vdir_t tmp = {
        .fs = NULL,
        .n_children = 0,
        .children = NULL,
        .path = malloc(strlen(path) + 1),
    };

    // path argument might not be in
    // cannonical form
    simplify_path(tmp.path, path);
    vdir_t *new = insert_vdir(&tmp);

    // if it cannot be inserted
    if (!new)
    {
        log_warn(
            "cannot mount %s,"
            "impossible to create virtual directory %s",
            part->name,
            tmp.path);

        free(tmp.path);
        return 0;
    }

    fs_t *fs = fat32_mount(part);

    if (!fs)
    {
        log_warn("cannot mount %s, unrecognized format", part->name);
        return 0;
    }

    new->fs = fs;

    file_handle_t* f = vfs_open_file("/fs/boot/limine.cfg");
    assert(f);

    assert(!vfs_seek_file(f, 512, SEEK_SET));
    log_warn("FILE SIZE = %u", vfs_tell_file(f));
    //vfs_seek_file(f, 0, SEEK_SET);

    char buf[26];
    int read = 0;
    int i = 0;
    while ((read = vfs_read_file(buf, 1, 25, f)))
    {
        buf[read] = 0;
        puts(buf);
        i++;
        // log_debug("%u read %u", i++, read);
    }
    vfs_close_file(f);
    log_debug("fg");

    return 1;
}

int vfs_unmount(const char *path)
{
    // do stuf!
    char *pbuf = malloc(strlen(path) + 1);
    simplify_path(pbuf, path);

    vdir_t *vdir = get_fs_vdir(pbuf);

    if (!vdir)
        return 0;

    return unmount(vdir);
}

file_handle_t *vfs_open_file(const char *path)
{
    char *pathbuf = malloc(strlen(path) + 1);
    simplify_path(pathbuf, path);

    fast_dirent_t dirent;
    fs_t *restrict fs = open_dir(pathbuf, &dirent);

    free(pathbuf);

    if (!fs) // dirent not found
        return NULL;

    // file does not exist or isn't a file
    if (dirent.type != DT_REG)
        return NULL;

    // big allocation to allocate the
    // sector buffer too
    file_handle_t *handler = malloc(
        sizeof(file_handle_t) + fs->file_cursor_size
        // size of one sector
        + fs->file_access_granularity);

    log_warn("file_access_granularity = %u", fs->file_access_granularity);

    handler->file_size = dirent.reclen;
    handler->fs = fs;
    handler->file_offset = 0;
    // empty buffer
    handler->sector_offset = 0;
    handler->buffer_valid = 0;

    handler->sector_buff = (void *)handler +
                           sizeof(file_handle_t) + fs->file_cursor_size;

    file_t file = {
        .addr = dirent.ino,
        .fs = fs,
        .file_size = dirent.reclen,
    };

    fs->open_file(&file, handler->cursor);
    return handler;
}

void vfs_close_file(file_handle_t *handle)
{
    fs_t *fs = handle->fs;

    fs->close_file(handle);

    free(handle);
}

struct DIR *vfs_opendir(const char *path)
{
    /**
     * as they are in different structures,
     * we must both search for virtual entries
     * and fs entries.
     *
     */
    char *pathbuff = malloc(strlen(path) + 1);
    simplify_path(pathbuff, path);

    size_t n = 0;

    fast_dirent_t fdir;
    fs_t *fs = open_dir(pathbuff, &fdir);

    struct DIR *dir = NULL;

    if (fs)
    { // dir does exist
        dirent_t *list = read_dir(fs, fdir.ino, pathbuff, &n);

        dir = malloc(sizeof(struct DIR) + sizeof(dirent_t) * n);

        memcpy(dir->children, list, n * sizeof(dirent_t));

        fs->free_dirents(list);
    }

    ////////////////////////////////////////
    // now search for virtual entries
    ////////////////////////////////////////

    int vn;
    vdir_t **vents = get_vchildren(pathbuff, &vn);

    if (vn >= 0)
    { // the dir exists in the vfs

        // allocate the extra results
        dir = realloc(dir, sizeof(struct DIR) + sizeof(dirent_t) * (n + vn));

        dirent_t *list = dir->children + n;

        for (int i = 0; i < vn; i++)
        {
            list[i] = (dirent_t){
                .ino = 0,
                .reclen = 0,
                .type = DT_DIR,
            };

            const char *subpath = strrchr(vents[i]->path, '/');
            assert(subpath);

            strcpy(list[i].name, subpath + 1);
        }
        n += vn;

        if (vents)
            free(vents);
    }

    free(pathbuff);

    if (dir)
    {
        dir->len = n;
        dir->cur = 0;
    }

    return dir;
}


void vfs_closedir(struct DIR *dir)
{
    free(dir);
}


struct dirent *vfs_readdir(struct DIR *dir)
{
    if (dir->cur == dir->len)
        return NULL; // reached end of dir
    else
        return &dir->children[dir->cur++];
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

    // we are to read some bytes.
    // if the fs is cachable, and the 
    // granularity cache is invalid,
    // we have to fill it beforehand
    if(cachable && !stream->buffer_valid) {
        fs->read_file_sector(
            stream->fs,
            stream->cursor,
            buf
        );

        stream->buffer_valid = 1;
    }
        

    while (bsize)
    {
        unsigned remaining = file_size - stream->file_offset;

        unsigned n_ready = granularity - stream->sector_offset;

        if(n_ready > remaining)
            n_ready = remaining;

        if(!cachable) {
            // for sure, bsize != 0.
            // we have to fetch again
            // the sector
            // without advancing the cursor
            fs->read_file_sector(
                stream->fs,
                stream->cursor,
                buf);
        }


        if (n_ready >= bsize) {
            memcpy(
                ptr,
                buf + stream->sector_offset,
                bsize
            );

            ptr += bsize;
            stream->sector_offset += bsize;
            stream->file_offset += bsize;
            bsize = 0;

            // here,
            // n_ready = granularity - sector_offset >= bsize
            // <=> sector_offset + bsize <= g
            // when n_ready = bsize,
            // we set stream->sector_offset to granularity.
            //
            // for consistancy with writes,
            // sector_offset has to be strictly inferior to
            // granularity, when at least one read has been
            // done.
            // We therefore advance the cursor
            if (!cachable && stream->sector_offset == granularity) {
                stream->sector_offset = 0;
                fs->advance_file_cursor(fs, stream->cursor);
            }
        }
        else {
            if (n_ready) {
                memcpy(
                    ptr,
                    buf + stream->sector_offset,
                    n_ready
                );

                bsize -= n_ready;
                ptr += n_ready;
                stream->file_offset += n_ready;
            }

            if (n_ready) // the cache was valid
                fs->advance_file_cursor(fs, stream->cursor);

            if (cachable) {
                // iif the fs is cachable,
                // we can fetch the next file sector
                // for the next iteration

                // for coherency with writes,
                // the file cursor must always point
                // on the current read sector: we therefore
                // cannot advance the file cursor afterwards
                // like read(cur++)
                //
                // Instead, we have to increment beforehand.
                // we cannot advance the cursor like read(++cur)
                // as the first sector would be forgotten

                fs->read_file_sector(
                    stream->fs,
                    stream->cursor,
                    buf
                );
            }
            stream->sector_offset = 0;
        }
    }

    return nmemb;
}



size_t vfs_write_file(const void *ptr, size_t size, size_t nmemb,
                      file_handle_t *stream)
{

    assert(stream);
    assert(ptr);
    assert(0);
    /*
    void * const buf = stream->sector_buff;

    const fs_t *fs = stream->fs;
    uint64_t file_size = stream->file_size;

    unsigned granularity = fs->file_access_granularity;

    unsigned cachable = fs->cacheable;

    // no checking: we hope that the drive won't
    // ever be overflowed
    size_t bsize = size * nmemb;


    while(bsize) {

         // don't need no buffering
        if((stream->sector_offset == 0
         || stream->sector_offset == granularity)
            && bsize >= granularity) {
            int wr = fs->write_file_sector(
                    fs,
                    stream->cursor,
                    ptr,
                    granularity
            );

            assert(wr == (int)granularity);
            ptr += granularity;
            stream->file_offset += granularity;
            bsize -= granularity;
        }
        else {
            unsigned offset = stream->sector_offset;
            unsigned write_size = MIN(bsize, granularity - offset);

            memcpy(buf + offset, ptr, write_size);

            if(stream->file_offset == stream->file_size
                    && offset == 0) {
                // no need to read before writing
            }
            else if(!fs->cacheable || offset == granularity) {
                // need to

            }

            int wr = fs->write_file_sector(
                    fs,
                    stream->cursor,
                    ptr,
                    granularity
            );

            assert(wr == write_size);

            ptr += bsize;
            stream->file_offset += bsize;
            bsize = 0;
        }

        stream->file_size = MAX(stream->file_offset,
                                stream->file_size);
        unsigned remaining = file_size - stream->file_offset;


        unsigned n_ready = granularity - stream->sector_offset;

        if(n_ready > remaining)
            n_ready = remaining;

        if(!cachable)
            n_ready = 0;


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
            if(n_ready) {
                memcpy(
                    ptr,
                    buf + stream->sector_offset,
                    n_ready
                );

                bsize -= n_ready;
                ptr += n_ready;
                stream->file_offset += n_ready;
            }

            stream->sector_offset = 0;

            fs->read_file_sector(
                stream->fs,
                stream->cursor,
                buf);
        }
    }
*/
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
        absolute_offset += stream->file_size;
        break;
    case SEEK_SET:
        break;
    default:
        // bad whence parameter value
        return -1;
    }

    size_t block_size = stream->fs->file_access_granularity;

    uint64_t new_sector_id = absolute_offset / block_size;
    uint64_t old_sector_id = stream->file_offset / block_size;

    if (new_sector_id != old_sector_id)
    {
        log_warn("CHANGED SECTOR %u --> %u", old_sector_id, new_sector_id);
        // the seeked value is in another block.
        // We therefore need to locate the new one
        fs_t *fs = stream->fs;
        fs->seek(fs, stream->cursor,  new_sector_id, SEEK_SET);

        stream->buffer_valid = 0;

        // need to flush the cache content
        if(stream->fs->cacheable)
            
            fs->read_file_sector(fs, stream->cursor, stream->sector_buff);
    }
    // else, only the sector offset changed.
    // no need to access the fs

    stream->sector_offset = absolute_offset % block_size;
    stream->file_offset = absolute_offset;

    log_warn("NEW OFFSET %u", stream->sector_offset);

    // everything is fine: return 0
    return 0;
}

long vfs_tell_file(file_handle_t *restrict stream)
{
    return stream->file_offset;
}