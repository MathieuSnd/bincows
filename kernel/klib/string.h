#ifndef KSTRING_H
#define KSTRING_H

#include <inttypes.h>

#ifndef size_t
typedef unsigned long int size_t;
#endif
#ifndef NULL
#define NULL (void *)(0)
#endif
#ifndef __ATTR_PURE__ 
#define __ATTR_PURE__ __attribute__((__pure__))
#endif 


void * memccpy (void *, const void *, int, size_t);
void * memchr (const void *, int, size_t) __ATTR_PURE__;
int memcmp (const void *, const void *, size_t) __ATTR_PURE__;
void * memcpy (void *, const void *, size_t);
void * memmem (const void *, size_t, const void *, size_t) __ATTR_PURE__;
void * memmove (void *, const void *, size_t);
void * memrchr (const void *, int, size_t) __ATTR_PURE__;
void * memset (void *, int, size_t);



size_t strlen (const char* str)  __ATTR_PURE__;
size_t strnlen(const char* str, size_t n)  __ATTR_PURE__;
int    strcmp (const char* str1, const char* str2)  __ATTR_PURE__;
int    strncmp(const char* str1, const char* str2, size_t n) __ATTR_PURE__; 
char*  strchr (const char *s, int c)  __ATTR_PURE__;
char*  strrchr(const char *s, int c)  __ATTR_PURE__;
char*  strstr(const char *haystack, const char *needle)  __ATTR_PURE__;


char*  strcpy (char* dst, const char* src);
char*  strncpy(char* dst, const char* src, size_t  n);
char*  strcat(char *dest, const char *src);
char*  strncat(char *dest, const char *src, size_t n);




#endif//KSTRING_H