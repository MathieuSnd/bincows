#ifndef FBCOPY_H
#define FBCOPY_H

#include <stdint.h>
#include <stddef.h>

typedef void (* fbcopy_f) (const void* buf0, const void* buf1, 
                            void* hw_buf, size_t count);

/*
    scan the system's capability and return the best function
    to operate the copy.
    Basically, the function uses AVX512 when possible,
    or AVX2, or SSE.
*/
fbcopy_f get_fbcopy_f(void);


#endif