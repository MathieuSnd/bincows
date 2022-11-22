#include <dirent.h>
#include <stdio.h>
#include <string.h>


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

    // read arguments
    int long_format = 0; // -l
    const char* dirname = ".";

    for(int i = 1; i < argc; i++) {
        if(!strcmp(argv[i], "-l")) {
            long_format = 1;
        }

        else
            dirname = argv[i];
    }

    dir = opendir(dirname);

    if (dir == NULL) {
        printf("cannot open directory %s\n", dirname);

        return 1;
    }

    while ((ent = readdir(dir)) != NULL) {
        char* color_seq = "\x1b[0m";

        if(ent->d_type == DT_DIR) {
            color_seq = "\x1b[36m";
        }

        const char* fmt = "%s%s\x1b[0m\t";

        if(long_format) {
            fmt = "%s%s\x1b[0m\n";
            

            // print size
            char reclen[32];

            int s = snprintf(reclen, 32, "%ld", ent->d_reclen);

            int nspaces = 10 - s;

            if(nspaces < 0)
                nspaces = 0;

            char spaces[11];

            memset(spaces, ' ', nspaces);

            spaces[nspaces]  ='\0';

            printf("%s%s\t", spaces, reclen);
            }


        printf(fmt, color_seq, ent->d_name);


    }

    printf("\n");

    closedir(dir);


    return 0;
}
