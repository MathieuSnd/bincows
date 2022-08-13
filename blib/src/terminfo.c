#include <terminfo.h>
#include <stdio.h>
#include <unistd.h>

int terminfo_read(terminfo_t* info) {

    int rd = read(1, info, sizeof(terminfo_t));

    return (rd == sizeof(terminfo_t)) ? 0 : -1;
}
