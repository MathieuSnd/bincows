#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BUFF_SIZE 1024*1024

static void cat(FILE *f)
{
    
    int c;
    char* buff = malloc(BUFF_SIZE);

    long rd;

    do {
        rd = read(fileno(f), buff, BUFF_SIZE);
        rd = write(1, buff, rd);
    }
    while(rd > 0);


    free(buff);
}

int main(int argc, char* argv[]) {
    int i;
    if(argc == 1) {
        cat(stdin);
    }
    
    for (i = 1; i < argc; i++) {
        FILE* f = fopen(argv[i], "r");
        if (f == NULL) {
            printf("cat: cannot open %s\n", argv[i]);
            return 1;
        }
        cat(f);

        fclose(f);
    }
    return 0;
}
