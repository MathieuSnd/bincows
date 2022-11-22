#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>
#include "terminal.h"

typedef struct Image
{
    uint32_t w, h;
    uint32_t pitch;
    uint8_t bpp;
    void *pix;
} Image;

typedef struct
{
    int16_t x, y;
} Pos;

// no inherence in C ...
typedef struct
{
    int16_t x, y;
    uint16_t w, h;
} Rect;

const Image *getScreenImage(void);

void imageDraw(const Image *img, const Pos *srcpos, const Rect *dstrect);

void imageLower_blit(
    const Image *img,
    uint16_t srcx, uint16_t srcy,
    uint16_t dstx, uint16_t dsty,
    uint16_t width,
    uint16_t height);

// for 32-bit images
// (@TODO) for now it is for 32-bit images
//
// maps the black src pixels to the black color argument
// and all the others colors to the white px
// _                    white
//  |
//  |----------------   black
// 0 1 2   ...    255
// 1D color
void imageLower_blitBinaryMask(
    const Image *img,
    uint16_t srcx, uint16_t srcy,
    uint16_t dstx, uint16_t dsty,
    uint16_t width, uint16_t height,
    uint32_t black,
    uint32_t white);

void imageBlit(
    const Image *__restrict__ src,
    Image *__restrict__ dst,
    const Pos *srcpos,
    const Rect *dstrect);

// only handle 32bit
void imageFillRect(uint32_t color, const Rect *rect);

// only handle 32bit and 8bit image
Image *loadBMP(const void *rawFile);

// load 1 BIT/px image
// from a 24 bit/px image
void loadBMP_24b_1b(const void *rawFile, Image* dst);

// frees the pix buffer
void bmp_free(Image* i);



void blitchar(void*, unsigned,
              const struct Image *charset,
              char c, uint32_t fg_color, uint32_t bg_color,
              uint16_t dstx, uint16_t dsty
);
void blitcharX2(void*, unsigned,
                const struct Image *charset,
                char c, uint32_t fg_color, uint32_t bg_color,
                uint16_t dstx, uint16_t dsty
);

// non temporal equivalent functions
// taking 2 chars to fill entire non
// temporal cache lines
void nt_blitchar(const struct framebuffer_dev* dev,
              const struct Image *charset,
              char c0, uint32_t fg_color0, uint32_t bg_color0,
              char c1, uint32_t fg_color1, uint32_t bg_color1,
              uint16_t dstx0, uint16_t dsty
);



#endif