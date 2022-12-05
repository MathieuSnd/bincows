#ifndef TERMINAL_H
#define TERMINAL_H

#include <stddef.h>


struct screen_size {
    size_t width;
    size_t height;
};

struct screen_size load_screen_size(void);

#endif