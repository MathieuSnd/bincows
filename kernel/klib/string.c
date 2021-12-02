#include "string.h"


size_t  strlen(const char* str) {
    const char* ptr = str;

    int len = 0;

    while(*(ptr++) != '\0')
        len++;

    return len;
}


size_t  strnlen(const char* str, size_t n) {
    int len = 0;

    while(*(str++) != '\0' && n--)
        len++;

    return len;
}

char* strcpy(char* dst, const char* src) {
    char c;
    char* pdst = dst;

    do {
       c = *(src++);

        *(pdst++) = c;
    }
    while(c != '\0');

    return dst;
}

char* strncpy(char* dst, const char* src, size_t n) {
    char c;
    char* pdst = dst;
    
    do {
       c = *(src++);

        *(pdst++) = c;

        n--;
    }
    while(c != '\0' && n > 0);
    
    if(c != '\0')
        *pdst = '\0';
    dst = '\0';
    return dst;
}


int   strcmp(const char* str1, const char* str2) {
    int d;

    while(1) {
        char c1 = *(str1++);
        char c2 = *(str2++);
        
        d = c1-c2;

        if(!c1 || !c2 || d)
            break;
    }

    return d;
}

int   strncmp(const char* str1, const char* str2, size_t n) {
    int d;
    
    while(n-- > 0) {
        char c1 = *(str1++);
        char c2 = *(str2++);
        
        d = c1-c2;

        if(!c1 || !c2 || d)
            break;
    }

    return d;
}


const char* strchr (const char *_s, int c) {
    const char* s=_s;
    char curr;

    while(1) {
        curr = *s;

        if(curr == c)
            return s;
        else if(curr == '\0')
            break;
        
        s++;
    }

    return NULL;    
}


const char* strrchr(const char *s, int c) {
    char curr;

    const char* found = NULL;

    while(1) {
        curr = *s;

        if(curr == c)
            found = s;
        else if(curr == '\0')
            break;
        
        s++;
    }

    return found;    
}


const char *strstr(const char *haystack, const char *needle) {

    int i = 0;
    char c, ci;

    while(1) {

        ci = needle[i];

        if(ci == '\0')
            return haystack - i - 1;


        c = *(haystack++);

        if(c == ci)
            i++;
        else if(c == '\0')
            return NULL;
        else
            i = 0;
    }
}


char* strcat(char *dest, const char *src) {
    while(*(dest++)) ;

    return strcpy(dest, src);
}

char *strncat(char *dest, const char *src, size_t n) {
    char* ret = dest;

    while(*(dest++)) ;
    strncpy(--dest, src, n);
    
    return ret;
}


void * memccpy (void* dst, const void *src, int c, size_t n) {


    for(; n > 0; n--) {
        char d = *(char*)(src++) = *(char*)(dst++);

        if(d == c)
            return dst;
    }

    return NULL;
}

const void* memchr (const void *_buf, int _ch, size_t n) {
    uint8_t ch = *(uint8_t*)(&_ch);
    const uint8_t* buf=_buf;

    for(;n > 0; n--) {
        if(*buf == ch)
            return buf;
        buf++;
    }
    return NULL;
}

int memcmp (const void* _buf1, const void* _buf2, size_t n) {
    const uint8_t* buf1=_buf1;
    const uint8_t* buf2=_buf2;

    for(; n > 0; n--) {
        int d = *(buf1++) - *(buf2++);

        if(d != 0)
            return d;
    }

    return 0;
}


void * memcpy (void* restrict _dest, 
         const void* restrict _src, size_t n) {
    const uint8_t* src=_src;
    uint8_t*       dest=_dest;

    for(;n > 0; --n)
        *(dest++) = *(src++);
    
    return dest;
}

void * memset (void * _buf, int _ch, size_t n) {
    uint8_t ch = *(uint8_t*)(&_ch);
    uint8_t* buf = _buf;

    // first unaligned bytes
    for(;n > 0 && (uint64_t)buf % 8 != 0; --n)
        *(buf++) = ch;


    if(!n) return buf;

    if(n >= 8) {
        uint64_t ch64 = ch | (ch << 8);
        ch64 = ch64 | (ch64 << 16);
        ch64 = ch64 | (ch64 << 32);

        for(;n > 0; n -= 8) {
            *(uint64_t*)buf = ch64;
            buf += 8;
        }
    }

    // last unaligned bytes
    for(;n > 0 && (uint64_t)buf % 8 != 0; --n)
        *(buf++) = ch;

    return buf;
}


void * memmove (void* _dest, const void* _src, size_t n) {
    uint8_t*       dest=_dest;
    const uint8_t* src=_src;
    
    // check for overlapping
    if(src < dest && src + n > dest) {
        dest += n;
        src  += n;

        
        // backward copy
        for(;n > 0; --n)
            *(--dest) = *(--src);

        return _dest;
    }
    // not overlapping: just call memcpy
    else {
        return memcpy(dest, src, n);
    }
}
