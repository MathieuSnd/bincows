#ifndef KSTRING_H
#define KSTRING_H

#include <stdint.h>
#include <stddef.h>


#ifndef __ATTR_PURE__ 
#define __ATTR_PURE__ __attribute__((__pure__))
#endif 


void * memccpy (void * __restrict__, const void * __restrict__, int, size_t);
const void * memchr (const void *, int, size_t) __ATTR_PURE__;
int memcmp (const void *, const void *, size_t) __ATTR_PURE__;
void * memcpy (void * __restrict__, const void * __restrict__, size_t);
void * memset (void *, int, size_t);
//void * memmem (const void *, size_t, const void *, size_t) __ATTR_PURE__;
void * memmove (void *, const void *, size_t);
//void * memrchr (const void *, int, size_t) __ATTR_PURE__;



size_t strlen (const char* str)  __ATTR_PURE__;
size_t strnlen(const char* str, size_t n)  __ATTR_PURE__;
int    strcmp (const char* str1, const char* str2)  __ATTR_PURE__;
int    strncmp(const char* str1, const char* str2, size_t n) __ATTR_PURE__; 
const char*  strchr (const char *s, int c)  __ATTR_PURE__;
const char*  strrchr(const char *s, int c)  __ATTR_PURE__;
const char*  strstr(const char *haystack, const char *needle)  __ATTR_PURE__;


char*  strcpy (char* __restrict__ dst, const char* __restrict__ src);
char*  strncpy(char* __restrict__ dst, const char* __restrict__ src, size_t  n);
char*  strcat(char * __restrict__ dest, const char * __restrict__ src);
char*  strncat(char * __restrict__ dest, const char * __restrict__ src, size_t n);




#endif//KSTRING_H