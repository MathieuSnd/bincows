#include <stdio.h>

int main(int argc, char** argv) {
    for(int i = 1; i < argc; i++) {
        if(i != argc-1)   
            printf("%s ", argv[i]);
        else
            printf("%s\n", argv[i]);
    }

    if(argc == 1)
        printf("\n");
}