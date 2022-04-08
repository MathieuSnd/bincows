#include <dirent.h>
#include <alloc.h>
#include <stdio.h>

typedef struct DIR {
    int fd;
    // number of elements
    size_t len;
    
    // next element to be
    // read by vfs_readdir
    size_t cur;

    struct dirent dirent[0];
//    struct dirent dirent_buf;
} DIR;

DIR *opendir (const char* name) {
    int fd = open(name, 0, 0);

    if(fd < 0) {
        return NULL;
    }

    // read the dir
    size_t size = lseek(fd, 0, SEEK_END);
    
    lseek(fd, 0, SEEK_SET);

    if(size % sizeof(struct dirent) != 0) {
        return NULL;
    }


    DIR* dir = malloc(sizeof(DIR) + size);

    if(!dir) {
        // out of memory
        return NULL;
    }

    size_t n = read(fd, dir->dirent, size);

    if(n != size) {
        // read error
        free(dir);
        return NULL;
    }


    // fill the other fields
    dir->fd = fd;
    dir->len = size;
    dir->cur = 0;

    return dir;
}

int closedir (DIR* dirp) {
    int fd = dirp->fd;
    free(dirp);

    return close(fd);
}



struct dirent *readdir (DIR* dirp) {
    if(dirp->cur >= dirp->len) {
        return NULL;
    }

    struct dirent* dirent = &dirp->dirent[dirp->cur];

    dirp->cur++;

    return dirent;
}


void rewinddir (DIR* dirp) {
    dirp->cur = 0;
}


void seekdir (DIR* dirp, long int pos) {
    dirp->cur = (size_t)pos;
}

/* Return the current position of DIRP.  */
long int telldir (DIR* dirp) {
    return (long int)dirp->cur;
}


/* Return the file descriptor used by DIRP.  */
int dirfd (DIR* dirp) {
    return dirp->fd;
}
