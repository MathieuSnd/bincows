#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>

typedef struct Image {
    uint32_t w,h;
    uint32_t pitch;
    uint8_t bpp;
    void* pix;
} Image;


typedef struct {
    int16_t  x,y;
} Pos;


// no inherence in C ... 
typedef struct {
    int16_t  x,y;
    uint16_t w,h;
} Rect;


const Image* getScreenImage(void);

void initVideo(const Image* srceen);
void draw(const Image* img, const Pos* srcpos, const Rect* dstrect);
void blit(const Image* __restrict__  src, 
                Image* __restrict__ dst, 
          const Pos* srcpos, const Rect* dstrect);

Image* loadBMP(const void* rawFile);

#endif