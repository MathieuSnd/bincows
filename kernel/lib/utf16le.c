#include "utf16le.h"



void utf16le2ascii(
        char* __restrict__ ascii, 
        const uint16_t* __restrict__ utf16le,
        size_t len
) {
    unsigned char c;

    for(;len>0;--len) {
        c = (*utf16le++) & 0xff;
        if(c > 127)
            c = '?';

        *ascii++ = c;

        if(!c) {
            *ascii = '\0';
            break;
        }
    }
}