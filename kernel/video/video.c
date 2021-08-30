#include <stdbool.h>
#include "../klib/string.h"
#include "../klib/sprintf.h"
#include "video.h"
#include "../common.h"
#include "../memory/kalloc.h"
#include "../debug/assert.h"

#define MAX(X,Y) X > Y ? X : Y

#define BPP 4

static Image screen;

void initVideo(const Image* s) {
    assert(s != NULL);
    assert(s->bpp == 32);

    memcpy(&screen,s, sizeof(Image));
}

/**
 *  blit the image without any verification
 *  (faster than blit but riskier)
 */
inline void lower_blit(const Image* src, const Image* dst,
            uint16_t srcx,  uint16_t srcy,
            uint16_t dstx,  uint16_t dsty,
            uint16_t width, uint16_t height) {

// begining of the first line
    uint8_t* dst_line_start = dst->pix + dsty * dst->pitch
                                       + dstx * BPP;
    uint8_t* src_line_start = src->pix + srcy * src->pitch
                                       + srcx * BPP;

    size_t src_skip = src->pitch;
    size_t dst_skip = dst->pitch;

    size_t copy_size = width * BPP;

    kprintf("copy_size: %lu\n"
            "dst_skip:  %lu", copy_size, dst_skip);
                            

    for(size_t i = height+1; i > 0 ; i--) {
        /*
        memcpy(dst_line_start, 
               src_line_start,
               copy_size);
        */
       memset(dst_line_start, 0xffffffff, copy_size);
        src_line_start += src_skip;
        dst_line_start += dst_skip;
    }
}

void draw(const Image* img, const Pos* srcpos, const Rect* dstrect) {
    blit(img, &screen, srcpos, dstrect);
}

void blit(const Image* restrict src, Image* restrict dst, 
          const Pos*  srcpos, const Rect* dstrect) {
    uint32_t sxbegin,
             sybegin,
            
             dxbegin,
             dybegin;

    uint32_t  w = INT32_MAX,
              h = INT32_MAX;

    // no srcrect is given
    // lets assume we want to blit on the top left corner
    if(!srcpos) {
        sxbegin = 0;
        sybegin = 0;
    }
    else {
        sxbegin = MAX(srcpos->x, 0);
        sybegin = MAX(srcpos->y, 0);
    }

    // destitation pos and dims
    if(!dstrect) {
        dxbegin = 0;
        dybegin = 0;
    }
    else {
        w = dstrect->w;
        h = dstrect->h;

    // cases where dstrect->x < 0
    // or dstrect->y < 0
        int16_t x = dstrect->x, y = dstrect->y;
        if(x >= 0)
            dxbegin = x;
        else {
            dxbegin = 0;
            sxbegin += x;
            w       -= x;
        }

        if(y >= 0)
            dybegin = y;
        else {
            dybegin = 0;
            sybegin += y;
            h       -= y;
        }
    }

    // sizes correction
    if(sxbegin + w > src->w)
        w = src->w - sxbegin;
    if(sybegin + w > src->h)
        h = src->h - sybegin;
        

    if(dxbegin + w > dst->w)
        w = src->w - sxbegin;
    if(dybegin + h > dst->h)
        h = src->h - sybegin;

    lower_blit(src, dst, sxbegin, sybegin, dxbegin, dybegin, w,h);
}


Image* alloc_image(uint32_t width, uint32_t height, uint32_t bpp) {
    Image* ret = kmalloc(sizeof(Image));

    ret->w     = width;
    ret->h     = height;
    ret->pitch = (uint32_t)allign16(width * (bpp/8));

    ret->pix   = kmalloc(ret->pitch * height);

    return ret;
}

void free_image(Image* im) {
    assert(im != NULL);

    kfree(im->pix);
}


#define BMP_MAGIC (uint16_t)0x4D42

/**
 * 
 * header format for a BMP file
 * 
 **/
struct BMPFileHeader {
    uint16_t magic;      // should be eq to BMP_MAGIC
    uint32_t size;       // size of the entire file (in bytes)
    uint16_t reserved1;  //
    uint16_t reserved2;  //
    uint32_t body_offset;// byte offset of the pixel body
    uint32_t header_size;// should be eq to sizeof(BMPFileHeader)
    int32_t w;           // BMP width in pixels
    int32_t h;           // BMP height in pixels
    uint16_t one;        // should be eq to 1
    uint16_t bpp;        // bits per pixel
} __packed;


inline bool check_BMP_header(const struct BMPFileHeader* header) {
    return header         != NULL      &&
           header->magic  != BMP_MAGIC &&
           header->w      >  0         &&
           header->h      >  0         &&
           header->one    == 1         ;
}



#define PRINT_VAL(v) kprintf(__FILE__ ":%d:%s::" \
                            #v "=%4lx\n", __LINE__,__func__,v);
#define PRINT_struct(v,s) kprintf("%x:\t" #v "=%ld\n", \
                            (size_t)&v-(size_t)s, v);


Image* loadBMP(const void* rawFile) {
    // the header should be at the beginning
    const struct BMPFileHeader* header = rawFile;


    if(check_BMP_header(header))
        return NULL;

    int32_t w = header->w;
    int32_t h = header->h;
    const uint8_t* srcpix24  = (const uint8_t*)rawFile + header->body_offset;

    Image* ret = alloc_image(w,h, 32);

    size_t bpitch = ret->pitch / 4;
    uint32_t* pix32 = ret->pix;

    for(size_t y = 0; y < ret->h; y++) {
        for(size_t x = 0; x < ret->w; x++) {
            const uint32_t* src_ptr  = (const uint32_t *)srcpix24 
                        + 3 * x + (ret->h-1-y) * 3 * h;

                        
            pix32[x + y * bpitch] = (*(const uint32_t *)src_ptr) & 0x00ffffff;
           
        }

    }

    PRINT_VAL(ret->w);
    PRINT_VAL(ret->h);
    PRINT_VAL(ret->pix);
    PRINT_VAL(ret->pitch);
    return ret;
}

