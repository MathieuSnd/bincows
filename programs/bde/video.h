#ifndef VIDEO_H
#define VIDEO_H

struct dev_video {
    unsigned width, height;
    unsigned pitch;
    unsigned bpp;
};

int fetch_video_info(struct dev_video* info);


typedef struct SDL_Surface SDL_Surface;

typedef struct {
    SDL_Surface* surface;
    void* malloced_ptr;

} fast_SDL_Surface;


// load bmp with the following constraints:
// - same pixel format as the target_format surface
// - 64-byte aligned (SIMD aligned copy)
fast_SDL_Surface* load_fast_bmp(const char* path, SDL_Surface* target_format);



#endif//VIDEO_H