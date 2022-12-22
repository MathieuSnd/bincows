#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdio_printf.h>
#include <time.h>

#include <SDL.h>

#include <immintrin.h>
#include <stdatomic.h>
#include <pthread.h>
#include <sys/events.h>

#include "fbcopy.h"


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


static SDL_Surface* screen;


typedef struct {
    SDL_Surface* surface;
    void* malloced_ptr;

} fast_SDL_Surface;


// load bmp with the following constraints:
// - same pixel format as the screen surface
// - 512 aligned (AVX-512 aligned copy)
fast_SDL_Surface* load_fast_bmp(const char* path) {

    SDL_Surface* bmp = SDL_LoadBMP(path);

    if(!bmp)
        return NULL;


    
    int    width      = bmp->w;
    int    height     = bmp->h;
    Uint32 src_format = bmp->format->format;
    void * src_pix    = bmp->pixels;
    int    src_pitch  = bmp->pitch;
    
    SDL_PixelFormat px;


    Uint32 dst_format = screen->format->format;
    
    int    dst_pitch  = screen->pitch;


    int sz = dst_pitch * height;
    
    // alloc enough to align on 512
    void* malloc_pix  = malloc(sz + 511);
    void* aligned_pix = (void*)(((uint64_t)malloc_pix + 511) & ~511llu);



    int res = SDL_ConvertPixels(
                            width,height,
                            src_format, src_pix, src_pitch,
                            dst_format, aligned_pix, dst_pitch);

    SDL_FreeSurface(bmp);

    if(res) {
        free(malloc_pix);
        return NULL;
    }



    fast_SDL_Surface* optimized = malloc(sizeof(fast_SDL_Surface) + sizeof(SDL_Surface));

    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormatFrom(
                            aligned_pix, width, height, 32, 
                            dst_pitch, screen->format->format
                            );

    *optimized = (fast_SDL_Surface) {
        .surface = surf,
        .malloced_ptr = malloc_pix,
    };

    assert((uint64_t)surf->pixels % 512 == 0);

    return optimized;
}

// double buffering
static SDL_Surface* fbuf[2];
static atomic_bool screen_doorbell;


void* update_fb_thread(void* unused) {
    (void) unused;

    size_t size = screen->pitch * screen->h;
    fbcopy_f fbcopy = get_fbcopy_f();

    // 30 FPS goal
    int min_frame_dur = 1000 * 1000 / 30; 
    
    uint64_t old_cl = time(NULL) / 1000;
    
    while(1) {
        // update the screen if requested
        
        int doorbell = atomic_exchange(&screen_doorbell, 0);
        if(doorbell) {
            fbcopy(fbuf[0]->pixels, fbuf[1]->pixels, screen->pixels, size);

            // switch double buffer
            // should be fine without locks
            SDL_Surface* tmp = fbuf[0];
            fbuf[0] = fbuf[1];
            fbuf[1] = fbuf[0];
        }

        uint64_t cl = time(NULL) / 1000;

        int64_t passed = cl - old_cl;
        if(passed < min_frame_dur) {
            // sleep for the rest of the frame
            usleep((min_frame_dur - passed));
        }
        old_cl = cl;

    }
}


int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_NOPARACHUTE);

    int res = fetch_video_info();
    if(res) {
        printf("%s: cannot fetch video information", argv[0]);
        return 1;
    }

    FILE* file = fopen("/mem/video", "r");
    void* framebuffer;

    assert(file);

    res = fread(&framebuffer, sizeof(framebuffer), 1, file);
    
    assert(res);
    assert(info.bpp == 32);

    screen = SDL_CreateRGBSurfaceFrom(
        framebuffer,
        info.width,
        info.height,
        32,
        info.pitch,
        0x00ff0000,
        0x0000ff00,
        0x000000ff,
        0x00000000
    );

    assert(screen != NULL);
    assert(screen->pitch != 0);


    // create double buffer
    for(int i = 0; i < 2; i++)
        fbuf[i] = SDL_CreateRGBSurfaceWithFormat(
            0,
            info.width,
            info.height,
            32,
            screen->format->format
        );


    assert(fbuf[0] != NULL);
    assert(fbuf[1] != NULL);

    // start the screen update thread
    pthread_t thread;
    pthread_create(&thread, NULL, update_fb_thread, NULL);


    // simple program
    fast_SDL_Surface* bg = load_fast_bmp("/home/wallpaper.bmp");
    assert(bg);
    SDL_Rect rect = {0,0,0,0};
    while(1) {
        SDL_BlitSurface(bg->surface, NULL, fbuf[0], &rect);
        screen_doorbell = 1;
        struct sys_event ev;
        
        int r = fread(&ev, sizeof(struct sys_event), 1, STDIN_FILENO);
        if(r != sizeof(struct sys_event)) {
            assert(0);
            return 1;
        }

        rect.x += 10;
    }

}
