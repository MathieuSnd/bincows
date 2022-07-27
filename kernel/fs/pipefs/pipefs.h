#pragma once
#include "../fs.h"

#define PIPE_SIZE 1024

fs_t* pipefs_mount(void);

typedef struct file_handle_pair {
    struct file_handle* in;
    struct file_handle* out;
} file_handle_pair_t;


// 0 on success
int create_pipe(file_handle_pair_t* ends);



