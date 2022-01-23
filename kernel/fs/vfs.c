#include "vfs.h"

#include "../lib/string.h"
#include "../lib/assert.h"
#include "../lib/logging.h"


#include "fat32/fat32.h"


static dirent_t vfs_root;



static 
int is_absolute(const char* path) {
    return path[0] == '/';
}


void init_vfs(void) {
    vfs_root.name[0]    = 0;
    vfs_root.type       = DT_DIR;
    vfs_root.children   = NULL;
    vfs_root.n_children = 0;
    vfs_root.file_size  = 0;

// virtual dir
    vfs_root.fs         = NULL;
}


dirent_t* find_child(dirent_t* dir, const char*  name) {
    for(int i = 0; i < dir->n_children; i++) {

        dirent_t* d = &dir->children[i];

        if(!strcmp(d->name, name))
            return d;
    }

    // 404 not found
    return NULL;
}

static dirent_t* add_dir_entry(
        dirent_t* restrict dir, 
        unsigned char type,
        const char* name) {

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


dirent_t* vfs_open_dir(const char* path, int create) {
    assert(is_absolute(path));

    char* buf = malloc(strlen(path));

    strcpy(buf, path);


    char* sub = strtok(buf, "/");

    dirent_t* dir = &vfs_root;
    dirent_t* d = dir;


    while(sub != NULL) {
        d = find_child(dir, sub);
        if(!d) {
            if(create)
                d = add_dir_entry(dir, DT_DIR, sub);
            else {
            
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



void log_tree(dirent_t* dir, int level) {
    
    for(int i = 0; i < level+1; i++)
        puts("--");
    
    printf(" %u - %s\n", dir->type, dir->name);

    if(dir->type != DT_DIR)
        return;
    if(!strcmp(dir->name, ".") || !strcmp(dir->name, ".."))
        return;

    int n = 0;

    if(dir->type == DT_DIR && !dir->children && dir->fs) {
        fat32_read_dir(dir->fs, dir, &n);
    }


    for(int i = 0; i < dir->n_children; i++)
        log_tree(&dir->children[i], level+1);

    // free(children);
}


int mount(disk_part_t* part, const char* path) {
    dirent_t* dir = vfs_open_dir(path, 1);

    fs_t* fs = fat32_detect(part, dir);

    if(!fs) {
        log_warn("cannot mount %s, unrecognized format", part->name);
        return 0;
    }

    log_tree(&vfs_root, 0);

    dirent_t* d = vfs_open_file("/a/boot/limine.cfg");

    assert(d);

    vfs_close_file(d);

    return 1;
}


void* vfs_open_file(const char* filename) {
    dirent_t* dir = vfs_open_dir(filename, 0);

    if(!dir || dir->type != DT_REG)
        return NULL;

    log_warn("dir=%x", dir->name);
    // virtual file (doesn't make sens)
    if(!dir->fs)
        return NULL; 
    
    return dir->fs->open_file(dir);
}


void vfs_close_file(void* handle) {
    dirent_t** base_handle = handle;

    fs_t* fs = (**base_handle).fs;
    
    fs->close_file(handle);
}

