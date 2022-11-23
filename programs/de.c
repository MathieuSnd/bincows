#include <stdio.h>
#include <string.h>
#include <assert.h>


struct dev_video {
    unsigned width, height;
    unsigned pitch;
    unsigned bpp;
} info;


int fetch_video_info(void) {
    FILE* f = fopen("/dev/video", "r");


    int r = fread(&info, 1, sizeof(info), f);

    if(r != sizeof(info)) {
        return -1;
    }
    
    // check that the structure size is right
    fseek(f, 0, SEEK_END);

    if(ftell(f) != sizeof(info))
        return -1;

    return 0;

}


int main(int argc, char** argv) {
    //int res = fetch_video_info();
    int res = 0;
    if(res) {
        printf("de fatal error: cannot fetch video information");
    }


    FILE* file = fopen("/mem/video", "r");
    void* framebuffer;

    assert(file);

    int r = fread(&framebuffer, sizeof(framebuffer), 1, file);

    assert(r);
    fclose(file);


//    while(1) {
//        //memset(framebuffer, 0xff, info.pitch * (info.height + 1));
//    }

}
