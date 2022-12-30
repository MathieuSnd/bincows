#include <unistd.h>
#include <bc_extsc.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>

struct shm_pair {
    int fd;
    char* name;
};


static int npairs;
struct shm_pair* pairs;


static
char* filename(const char* name) {
    const char* str1 = "/mem";

    // return str1 + name
    char* ret = malloc(strlen(str1) + strlen(name) + 1);

    strcpy(ret, str1);
    strcat(ret, name);

    return ret;
}


int shm_open(const char *name, int oflag, mode_t mode) {
    char* fn = filename(name);
    
    int res = open(fn, oflag, mode);

    if(res < 0) {
        free(fn);
        return -1;
    }



    pairs = realloc(pairs, (npairs + 1) * sizeof(*pairs));
    pairs[npairs++] = (struct shm_pair) {
        .fd = res,
        .name =fn
    };


    return res;
}


int shm_unlink(const char *name) {
    for(int i = 0; i < npairs; i++) {
        if(!strcmp(pairs[i].name, name)) {
            close(pairs->fd);
            free(pairs[i].name);
            memmove(
                pairs + i,
                pairs + i + 1,
                npairs - i - 1
            );

            return 0;
        }
    }
    return -1;
}

void *mmap(void *addr, size_t length, int prot, int flags,
            int fd, off_t offset)
{
    (void) addr;
    (void) length;
    (void) prot;
    (void) flags;
    (void) offset;

    void* map_addr;

    int pos = lseek(fd, 0, SEEK_CUR);

    if(pos != 0) {
        return NULL;
    }


    int n = read(fd, &map_addr, sizeof(map_addr));

    if(n != sizeof(map_addr)) {
        return NULL;
    }

    return map_addr;
}

int munmap(void *addr, size_t length) {
    (void) addr;
    (void) length;
    return -1;
}

