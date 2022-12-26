#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

#include <stdint.h>
#include <stddef.h>


typedef unsigned long long ino_t;
typedef unsigned long long off_t;
typedef off_t off64_t;

typedef int64_t ssize_t;
#define SSIZE_MAX INT64_MAX


typedef int pid_t;


#endif//_SYS_TYPES_H