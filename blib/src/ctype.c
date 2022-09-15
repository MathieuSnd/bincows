#include <ctype.h>

int isxdigit(int c) {
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int isdigit(int c) {
    return c >= '0' && c <= '9';
}

int isalpha(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int isalnum(int c) {
    return isdigit(c) || isalpha(c);
}

int islower(int c) {
    return c >= 'a' && c <= 'z';
}

int isupper(int c) {
    return c >= 'A' && c <= 'Z';
}

int isblank(int c) {
    return c == ' ' || c == '\t';
}

int iscntrl(int c) {
    return c >= 0x00 && c <= 0x1F;
}

int isgraph(int c) {
    return c >= '!' && c <= '~';
}

int isprint(int c) {
    return c >= ' ' && c <= '~';
}

int ispunct(int c) {
    return c >= '!' && c <= '/' || c >= ':' && c <= '@' || c >= '[' && c <= '`' || c >= '{' && c <= '~';
}

int isspace(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

int isascii(int c) {
    return c >= 0x00 && c <= 0x7F;
}



int tolower(int c) {
    if(c >= 'A' && c <= 'Z')
        return c + 32;
    return c;
}

int toupper(int c) {
    if(c >= 'a' && c <= 'z')
        return c - 32;
    return c;
}
