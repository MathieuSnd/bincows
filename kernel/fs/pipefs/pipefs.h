#pragma once
#include "../fs.h"

#define PIPE_SIZE 1024

fs_t* pipefs_mount(void);

typedef struct file_pair {
    file_t in;
    file_t out;
} file_pair_t;


// 0 on success
int create_pipe(file_pair_t* ends);



