#pragma once


/**
 * @brief write msr to enable
 * syscalls
 * 
 */
void syscall_init(void);

typedef enum open_flags {
    O_RDONLY    = 0,
    O_WRONLY    = 1,
    O_RDWR      = 2,
    O_CREAT     = 4,
    O_EXCL      = 8,
    O_TRUNC     = 16,
    O_APPEND    = 32,
    O_NONBLOCK  = 64,
    O_DIRECTORY = 128,
    O_NOFOLLOW  = 256,
    O_DIRECT    = 512,
    O_NOATIME   = 1024,
    O_CLOEXEC   = 2048,
    O_DIR       = 4096,
} open_flags_t; 

#include "syscall_interface.h"

