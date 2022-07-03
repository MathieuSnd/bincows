#include "vfs.h"

#include "../lib/string.h"
#include "../lib/assert.h"
#include "../lib/logging.h"
#include "../lib/dump.h"
#include "../lib/sprintf.h"
#include "../lib/panic.h"
#include "../lib/registers.h"
#include "../lib/math.h"
#include "../acpi/power.h"

#include "../memory/heap.h"
#include "fat32/fat32.h"
#include "devfs/devfs.h"

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


static int unmount(vdir_t *vdir);


static int is_absolute(const char *path)
{
    return path[0] == '/';
}


void simplify_path(char *dst, const char *src)
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
    unsigned len = strlen(path) / 2;
    uint16_t res = 0;
    const uint8_t *dwp = (const uint8_t *)path;

    for (unsigned i = 0; i < len; i++)
    {
        uint32_t entropy_bits = dwp[2 * i] ^ (dwp[2 * i + 1] << 4);

        entropy_bits ^= (entropy_bits << 4);

        res = (res ^ (entropy_bits + i)) & 8191;
    }
    return res;
}

// recursively destruct the entire
// vdir tree
// does not remove the root path
static void free_vtree(vdir_t *root)
{
    for (unsigned i = 0; i < root->n_children; i++)
        free_vtree(root->children + i);


    if(root->n_children) {
        free(root->children);
        root->n_children = 0;
    }
    log_info("unmounting %s", root->path);

    // unmount mounted entries
    if (root->fs) {
        int res = unmount(root);
        
        assert(res);
    }
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
                free(vdir->path);

                memmove(
                    vd->children + i,
                    vd->children + i + 1,
                    (vd->n_children - i - 1) * sizeof(vdir_t)
                );
                


                vd->n_children--;
                vd->children = realloc(vd->children, vd->n_children);

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
static int unmount(vdir_t *vdir) {
    assert(vdir);
    assert(vdir->fs);


    fs_t *fs = vdir->fs;


    if (vdir->n_children)
    {
        log_warn("cannot unmount partition %s: it is not a leaf partition", fs->part->name);

        return 0;
    }

    if (fs->n_open_files != 0)
    {
        log_warn(
            "cannot unmount partition %s: %u open files",
            vdir->path, fs->n_open_files);

        return 0;
    }
    
    // assert that the partition 
    // is actually mounted

    if(fs->part) {
        assert(fs->part->mount_point);
        
        free(fs->part->mount_point);

        // set the partition not mounted
        fs->part->mount_point = NULL;
    }

    free(fs->name);

        

    if (vdir == &vfs_root)
        vfs_root.fs = NULL;
    else {
        if(vdir->children)
            return 0;
        
        remove_vdir(vdir);
    }

    


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
}



void vfs_cleanup(void) {
    assert(interrupt_enable());
    
    vfs_lazy_flush();

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
 * @brief add a cache entry
 * with given path fs and dirent data
 * @warning the path argument will belong
 * to the cache entry. It will be freed
 * when the cache entry will get removed.
 * It shouldn't be freed beforehand
 * 
 * @param path path of the cache entry, in
 * canonical form
 * @param fs the dirent fs
 * @param dent dirent data
 */
static 
void add_cache_entry(char* path, fs_t* fs, fast_dirent_t* dent) {

    uint16_t hash = path_hash(path) & (cache_size - 1);

    dir_cache_ent_t *cache_ent = &cached_dir_list[hash];

    if (is_cache_entry_present(cache_ent))
        free_cache_entry(cache_ent);

    cache_ent->cluster   = dent->ino;
    cache_ent->file_size = dent->file_size;
    cache_ent->type      = dent->type;
    cache_ent->rights    = dent->rights;
    cache_ent->fs        = fs;
    cache_ent->path      = path;
}


/**
 * @brief wrapper around fs->read_dir
 * function. It adds the entries in the 
 * vfs cache
 * 
 * do not forget to call
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

        add_cache_entry(path, fs, &dent->fast);
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
 * find a child of a given directory in the fs 
 * with a given name 
 * 
 * the cache strategy we use here is a predictive one:
 * when opening a file, we suppose that its siblings
 * are likely to be opened soon afterwards
 * we then put them all in the cache.
 * 
 * @param fs the fs
 * @param parent the parent dirent
 * @param child (output) the 
 * @param name requested name
 * @return int non 0 value if the
 * requested son exists according 
 * to the fs
 */
static int find_fs_child(
        fs_t *restrict          fs,
        fast_dirent_t* restrict parent,
        fast_dirent_t* restrict child,
        const char*             name,
        const char*             parent_path
) {
    assert(parent->type == DT_DIR);
    
    size_t n;

    int found = 0;

    dirent_t *ents = read_dir(fs, parent->ino, parent_path, &n);

    // should be something like this
    // to avoid putting too much files 
    // in cache (we would only put the files we open
    // instead of each of its siblings):
    //
    //dirent_t *ents = fs->read_dir(fs, parent->ino, &n);


    for (unsigned i = 0; i < n; i++)
    {
        if (!strcmp(ents[i].name, name))
        {

            child->ino = ents[i].ino;
            child->file_size = ents[i].file_size;
            child->type = ents[i].type;
            child->rights = ents[i].rights;

            assert(child->type == DT_DIR || child->rights.value != 0);

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
    uint64_t rf = get_rflags();
    _cli();


    // @todo: add vfs cache spinlock:
    // aquire it there, and free it after 
    // doing stuf with the returned entry
    assert(is_absolute(path));

    assert(cache_size);

    uint16_t hash = path_hash(path) & (cache_size - 1);

    // check cache
    dir_cache_ent_t *ent = &cached_dir_list[hash];
    if (ent->path == NULL) {// not present entry
        //log_debug("VFS cache miss for %s (hash %u): not present", path, hash);
        set_rflags(rf);
        return NULL;
    }

    if (!strcmp(ent->path, path)) {// right entry (no collision)
        //log_debug("VFS cache hit for %s (hash %u)", path, hash);
        set_rflags(rf);
        return ent;
    }

    //log_debug("VFS cache miss for %s (hash %u): conflit", path, hash);

    set_rflags(rf);
    return NULL;
}


fs_t *vfs_open(const char *path, fast_dirent_t *dir)
{
    assert(interrupt_enable());

    // this buffer belongs to the cache entry 
    // in case of miss. Otherwise, it is
    // released before returning.
    char* pathbuf = malloc(strlen(path)+1);

    simplify_path(pathbuf, path);
    assert(is_absolute(pathbuf));


    // check cache
    dir_cache_ent_t *ent = get_cache_entry(pathbuf);
    if (ent) { // present entry
        dir->ino = ent->cluster;
        dir->file_size = ent->file_size;
        dir->type = ent->type;
        dir->rights = ent->rights;

        free(pathbuf);

        return ent->fs;
    }


    // cache miss!


    vdir_t *vdir = get_fs_vdir(pathbuf);


    if (!vdir)
    {
        free(pathbuf);
        // there is no way the dir could
        // exist
        return FS_NO;
    }

    fs_t *fs = vdir->fs;


    // only keep the FS part
    // of the path
    char* parent_path = pathbuf + strlen(vdir->path);


    fast_dirent_t cur = {
        .ino = fs->root_addr,
        .file_size = 0,
        .type = DT_DIR,
    };

    // in case vdir is the root
    // the parent path is '/'
    // and therefore finishes with a 
    // '/'. Then, parent_path does
    // not begin with '/' which is
    // mendatory for the following code
    if(*parent_path != '/') {
        parent_path--;

        if(parent_path[1] == 0) {
            *dir = cur;
    
            free(pathbuf);

            return fs;
        }
    }


    char* name_end = parent_path; 

    while(1) {        
        // 'parent_path' is the
        // entire path
        if(! name_end) break; 

        // this is the first iteration
        // and the path is "": this is the root
        if(! *name_end) break; 

        // this should point to the 
        // '/' separator between the
        // end of the parent path and
        // the beginning of the son
        // file name
        char* name_sep = name_end;

        name_end = (char*) strchr(name_sep + 1, '/');


        if(name_end)
            *name_end = '\0';


        fast_dirent_t child;

        // first check the cache
        dir_cache_ent_t* ent = get_cache_entry(pathbuf);

        if(ent) {
            // hit!!
            assert(ent->fs == fs);

            cur.ino       = ent->cluster;
            cur.file_size = ent->file_size;
            cur.type      = ent->type;
            cur.rights    = ent->rights;  

            // restore the path buffer
            if(name_end)
                *name_end = '/';

            continue;
        }

        // miss

        *name_sep = '\0';

        if (!find_fs_child(fs, &cur, &child, name_sep + 1, pathbuf))
        {
            // the child does not exist
            free(pathbuf);
        
            return FS_NO;
        }

        // restore the path buffer
        *name_sep = '/';

        if(name_end)
            *name_end = '/';


        cur.ino       = child.ino;
        cur.file_size = child.file_size;
        cur.type      = child.type;
        cur.rights    = child.rights;

        if(cur.type == DT_REG)
            assert(cur.rights.value);
    }


    dir->ino = cur.ino;
    dir->file_size = cur.file_size;
    dir->type = cur.type;
    dir->rights = cur.rights;

    assert(dir->type == DT_DIR || dir->rights.value != 0);

    // we missed, so 
    // add a cache entry
    add_cache_entry(pathbuf, fs, dir);

    return fs;
}


// if it cannot be inserted, return NULL
static vdir_t* emplace_vdir(const char* path) {

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


    // if we are mounting on the root dir,
    // we should return it if it is not
    // already monted
    if (!strcmp(tmp.path, "/")) {
        free(tmp.path);

        if(vfs_root.fs == NULL)
            return &vfs_root;
        else
            return NULL;
    }

    vdir_t *new = insert_vdir(&tmp);

    if(!new)
        free(tmp.path); 

    return new;
}



// asserts that fs is well formed
static void check_fs_struct(fs_t *fs) {
    assert(fs->add_dirent != NULL);
    assert(fs->read_dir != NULL);
    assert(fs->read_file_sectors != NULL);
    assert(fs->write_file_sectors != NULL);
    assert(fs->truncate_file != NULL);
    assert(fs->update_dirent != NULL);
    assert(fs->name != NULL);
}



int vfs_mount(disk_part_t *part, const char *path)
{
    vdir_t* new = emplace_vdir(path);

    // if it cannot be inserted
    if (!new)
    {
        log_warn(
            "cannot mount %s,"
            "impossible to create virtual directory %s",
            part->name,
            path);

        return 0;
    }

    fs_t *fs = fat32_mount(part);

    if (!fs)
    {
        log_warn("cannot mount %s, unrecognized format", part->name);
        return 0;
    }

    new->fs = fs;
    part->mount_point = strdup(new->path);

    check_fs_struct(fs);
    return 1;
}



int vfs_mount_devfs(void) {

    const char* path = "/dev";

    vdir_t* new = emplace_vdir(path);

    // if it cannot be inserted
    if (!new)
    {
        log_warn(
            "cannot mount %s,"
            "impossible to create virtual directory %s",
            "devfs",
            path);

        return 0;
    }

    fs_t *fs = devfs_mount();

    if (!fs)
    {
        log_warn("cannot mount %s, unrecognized format", path);
        return 0;
    }

    new->fs = fs;

    check_fs_struct(fs);

    return 1;
}



int vfs_unmount(const char *path)
{
    log_info("vfs_unmount(%s)", path);
    // do stuf!
    char *pbuf = malloc(strlen(path) + 1);
    simplify_path(pbuf, path);

    vdir_t *vdir = get_fs_vdir(pbuf);

    free(pbuf);

    if (!vdir)
        return 0;

    return unmount(vdir);
}



int vfs_create(const char* path, int type) {
    char* pathbuf = malloc(strlen(path) + 1);

    simplify_path(pathbuf, path);

    char* last_sep = (char*)strrchr(pathbuf, '/');

    if(!last_sep) {
        // bad
        free(pathbuf);
        return 1;
    }


    *last_sep = 0;

    fast_dirent_t parentdir;
    fs_t* fs = vfs_open(pathbuf, &parentdir);

    free(pathbuf);

    // if the parent directory
    // is not a directory, or does not
    // exist
    if(fs == FS_NO || parentdir.type != DT_DIR)
        return 1;
    
    // handle the virtual case

    // actually add the entry

    
    assert(0);
    //fs->add_dirent()
    (void) type;

    return 0;
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

    struct DIR *dir = NULL;
    fast_dirent_t fdir;
    fs_t *fs = vfs_open(pathbuff, &fdir);


    if(fs == FS_NO) {
        // the path does not exist
        free(pathbuff);
        return NULL;
    }

    else if(fs)
    { // dir does exist
        if(fdir.type != DT_DIR) {
            free(pathbuff);
            return NULL;
        }
        
        dirent_t *list = read_dir(fs, fdir.ino, pathbuff, &n);

        dir = malloc(sizeof(struct DIR) + sizeof(dirent_t) * n);

        memcpy(dir->children, list, n * sizeof(dirent_t));

        fs->free_dirents(list);
    }
    // else, the path is virtual

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
                .file_size = 0,
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

struct DIR* vfs_dir_dup(struct DIR* dir) {
    struct DIR* new = malloc(sizeof(struct DIR) + sizeof(dirent_t) * dir->len);
    memcpy(new->children, dir->children, dir->len * sizeof(dirent_t));
    new->len = dir->len;
    new->cur = dir->cur;
    return new;
}


struct dirent *vfs_readdir(struct DIR *dir)
{
    if (dir->cur == dir->len)
        return NULL; // reached end of dir
    else
        return &dir->children[dir->cur++];
}


int vfs_update_metadata_cache(
        const char* path, 
        uint64_t addr, 
        size_t file_size
) {
    char* pathbuf = malloc(strlen(path)+1);

    simplify_path(pathbuf, path);
    assert(is_absolute(pathbuf));

    uint64_t rf = get_rflags();
    _cli();

    // update cache if hit
    dir_cache_ent_t *ent = get_cache_entry(pathbuf);

    assert(ent);

    if (ent) {// hit
        // it should be a file
        assert(ent->type == DT_REG);

        ent->cluster   = addr;
        ent->file_size = file_size;
    }

    set_rflags(rf);
    
    free(pathbuf);

    return 0;
}


int vfs_update_metadata_disk(
        const char* path, 
        uint64_t addr, 
        size_t file_size
) {
    assert(interrupt_enable());

    char* pathbuf = malloc(strlen(path)+1);

    simplify_path(pathbuf, path);
    assert(is_absolute(pathbuf));

    // first open the parent and compute
    // the file name
    char* parent_path = pathbuf;

    char* file_name = strrchr(pathbuf, '/');
    assert(file_name);


    if(file_name == pathbuf) {
        // root
        parent_path = "/";
    }
    else 
        *file_name = 0;
    
    file_name++;

    // here, pathbuf = '/a/b/parent'
    // and file_name = 'name'

    fast_dirent_t parent_dirent;
    fs_t* fs = vfs_open(parent_path, &parent_dirent);

    assert(fs != FS_NO);

    if(!fs) {
        // parent is a virtual directory
        free(pathbuf);
        return 0;
    }

    assert(parent_dirent.type == DT_DIR);
    assert(parent_dirent.file_size == 0);


    int ret = fs->update_dirent(fs, parent_dirent.ino, file_name, addr, file_size);
    free(pathbuf);

    return ret;
}
