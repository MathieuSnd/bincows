#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <SDL.h>
#include "video.h"



int fetch_video_info(struct dev_video* info) {

    FILE* f = fopen("/dev/video", "r");


    int r = fread(info, 1, sizeof(*info), f);

    if(r != sizeof(*info)) {
        return -1;
    }
    
    // check that the structure size is right
    fseek(f, 0, SEEK_END);

    if(ftell(f) != sizeof(*info))
        return -1;

    return 0;
}



fast_SDL_Surface* load_fast_bmp(const char* path, SDL_Surface* target_format) {
    SDL_Surface* bmp = SDL_LoadBMP(path);

    if(!bmp)
        return NULL;


    
    int    width      = bmp->w;
    int    height     = bmp->h;
    Uint32 src_format = bmp->format->format;
    void * src_pix    = bmp->pixels;
    int    src_pitch  = bmp->pitch;
    
    SDL_PixelFormat px;


    Uint32 dst_format = target_format->format->format;
    
    int    dst_pitch  = target_format->pitch;


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
                            dst_pitch, target_format->format->format
                            );

    *optimized = (fast_SDL_Surface) {
        .surface = surf,
        .malloced_ptr = malloc_pix,
    };

    assert((uint64_t)surf->pixels % 512 == 0);

    return optimized;
}

