#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    printf("argc=%d\n", argc);
    int i;
    for (i = 1; i < argc; i++) {
        printf("argv[%d]=%s\n", i, argv[i]);
        FILE* f = fopen(argv[i], "r");
        if (f == NULL) {
            printf("cat: cannot open %s\n", argv[i]);
            return 1;
        }
        int c;
        char* buff = malloc(32);
        

        while (fread(buff, 1, 32, f)) {
            write(1, buff, 32);

        }
        fclose(f);
    }
    return 0;
}
