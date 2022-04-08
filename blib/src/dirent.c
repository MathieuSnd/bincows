#include "dirent.h"

typedef struct DIR {
    int fd;
    struct dirent* dirent;
//    struct dirent dirent_buf;
} DIR;


DIR *opendir (const char *__name) {

}