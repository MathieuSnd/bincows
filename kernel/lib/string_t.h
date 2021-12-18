#pragma once
#include <stddef.h>

#include "../memory/heap.h"

// this file defines
// the string_t structure

typedef struct {
    const char* ptr;
    char freeable;
} string_t;

static inline string_t string_alloc(size_t len) {
    return (string_t){
        .ptr = (char*)malloc(len),
        .freeable = 1
    };
}

static inline void string_free(string_t* s) {
    if(s->freeable)
        free((void*)s->ptr);
}