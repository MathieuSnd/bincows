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

    *pdst = '\0';

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


char* strchr (const char *s, int c) {
    char curr;

    while(1) {
        curr = *s;

        if(curr == c)
            return s;
        else if(curr = '\0')
            break;
        
        s++;
    }

    return NULL;    
}


char* strrchr(const char *s, int c) {
    char curr;

    char* found = NULL;

    while(1) {
        curr = *s;

        if(curr == c)
            found = s;
        else if(curr = '\0')
            break;
        
        s++;
    }

    return found;    
}


char *strstr(const char *haystack, const char *needle) {
    char* ptr = haystack;

    int i = 0;
    char c, ci;

    do {

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
    while(*(dest++)) ;

    return strncpy(dest, src, n);
}
