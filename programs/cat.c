#include <stdio.h>
#include <stdlib.h>


static void cat(FILE *f)
{
    
    int c;
    char buff[31];

    buff[30] = 0;

    while (fread(buff, 1, 1, f))
        write(1, buff, 1);
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
