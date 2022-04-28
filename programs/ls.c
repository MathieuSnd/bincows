#include <dirent.h>
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



int main(int argc, char** argv) {
    DIR* dir;
    struct dirent* ent;
    if (argc == 1) {
        dir = opendir(".");
    } else {
        dir = opendir(argv[1]);
    }
    if (dir == NULL) {
        printf("cannot open directory\n");

        return 1;
    }

    while ((ent = readdir(dir)) != NULL) {
        printf("%s\t", ent->d_name);
    }

    printf("\n");

    closedir(dir);


    return 0;
}
