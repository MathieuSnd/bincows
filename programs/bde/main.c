#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdio_printf.h>

#include <SDL.h>


struct dev_video {
    unsigned width, height;
    unsigned pitch;
    unsigned bpp;
} info;


int fetch_video_info(void) {
    SDL_Init(0);

    FILE* f = fopen("/dev/video", "r");


    int r = fread(&info, 1, sizeof(info), f);

    if(r != sizeof(info)) {
        return -1;
    }
    
    // check that the structure size is right
    fseek(f, 0, SEEK_END);

    if(ftell(f) != sizeof(info))
        return -1;

    printf("r = %u, %ux%ux%u\n",info.width,info.height,info.bpp);

    return 0;

}




int sigsegv(int sig) {
    for(;;);
}



int main(int argc, char** argv) {

    SDL_Init(SDL_INIT_NOPARACHUTE);


    int res = fetch_video_info();
    if(res) {
        printf("de fatal error: cannot fetch video information");
        return 1;
    }





    FILE* file = fopen("/mem/video", "r");
    void* framebuffer;

    assert(file);

    res = fread(&framebuffer, sizeof(framebuffer), 1, file);

    assert(res);
    assert(info.bpp == 32);

    SDL_Surface* screen = SDL_CreateRGBSurfaceFrom(
        framebuffer,
        info.width,
        info.height,
        32,
        info.pitch,
        0x00ff000000,
        0x0000ff0000,
        0x000000ff00,
        0x0000000000
    );

    assert(screen != NULL);


    SDL_Surface* bg = SDL_LoadBMP("/home/wallpaper.bmp");
    assert(bg != NULL);


    Uint32 color = 0;
    while(1) {
        int res = SDL_BlitSurface(bg, NULL, screen, NULL);

        assert(!res);
    }


    fclose(file);
}
