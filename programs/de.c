#include <stdio.h>
#include <string.h>
#include <assert.h>


int main(int argc, char** argv) {
    FILE* file = fopen("/mem/video", "r");
    void* framebuffer;

    assert(file);

    int r = fread(&framebuffer, sizeof(framebuffer), 1, file);

    assert(r);
    fclose(file);


    while(1) {
        memset(framebuffer, 0xff, 4 * 1920 * 500);
    }

}
