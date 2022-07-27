#include <terminfo.h>
#include <stdio.h>

int terminfo_read(terminfo_t* info) {

    int rd = fread(info, sizeof(terminfo_t), 1, stdout);

    printf("rd = %d", rd);

    return (rd == 1) ? 0 : -1;
}