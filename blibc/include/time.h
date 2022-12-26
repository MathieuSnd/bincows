#ifndef TIME_H
#define TIME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t time_t;

extern time_t time(time_t *t);

#ifdef __cplusplus
}
#endif

#endif // TIME_H