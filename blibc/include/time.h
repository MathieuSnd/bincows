#ifndef TIME_H
#define TIME_H

#include <stddef.h>
#include <stdint.h>

typedef uint64_t time_t;

extern time_t time(time_t *t);


#endif // TIME_H