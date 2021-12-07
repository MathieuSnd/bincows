#include <stdbool.h>

// for initialization
#include <stivale2.h>

#include "../lib/string.h"
#include "../lib/logging.h"
//#include "../memory/kalloc.h"
#include "../lib/common.h"
#include "../lib/assert.h"
#include "video.h"
#include "terminal.h"

#define MAX(X,Y) X > Y ? X : Y

#define BPP 4

static Image screen;

inline void lower_blit(const Image* src, const Image* dst,
            uint16_t srcx,  uint16_t srcy,
            uint16_t dstx,  uint16_t dsty,
            uint16_t width, uint16_t height);

void initVideo(const struct stivale2_struct_tag_framebuffer* fbtag, 
               void* frame_buffer_virtual_address) {
    assert(fbtag != NULL);
    

    screen  = (Image){
        .w    =        fbtag->framebuffer_width,
        .h    =        fbtag->framebuffer_height,
        .pitch=        fbtag->framebuffer_pitch,
        .bpp  =        fbtag->framebuffer_bpp,
        .pix  =        frame_buffer_virtual_address
    };
    
    assert(screen.bpp == 32);

}

void imageLower_blit(const Image* img,
                     uint16_t     srcx,  uint16_t srcy,
                     uint16_t     dstx,  uint16_t dsty,
                     uint16_t     width, uint16_t height) {

    lower_blit(img, &screen, srcx,srcy,dstx,dsty,width,height);
}

void imageLower_blitBinaryMask(
    const Image* img,
    uint16_t srcx,  uint16_t srcy,
    uint16_t dstx,  uint16_t dsty,
    uint16_t width, uint16_t height,
    uint32_t black, uint32_t white) {
    
    
    assert(img->bpp == 32);

// begining of the first line
    uint8_t* dst_ptr = screen.pix + dsty * screen.pitch
                                  + dstx * BPP;

    uint8_t* src_ptr = img->pix   + srcy * img->pitch
                                  + srcx * BPP;

    size_t copy_size = width * BPP;


    size_t dst_skip = screen.pitch - copy_size;
    size_t src_skip = img -> pitch - copy_size;

// assert that everything are 2-aligned
// so that we can process faster
    assert((size_t)dst_ptr   % 8 == 0);
    assert((size_t)src_ptr   % 8 == 0);

    assert((size_t)copy_size % 8 == 0);
    assert((size_t)dst_skip  % 8 == 0);
    assert((size_t)src_skip  % 8 == 0);
    

    uint32_t* src_ptr32 = (uint32_t *)src_ptr;
    uint32_t* dst_ptr32 = (uint32_t *)dst_ptr;


    
    for(size_t i = height+1; i > 0 ; i--) {

    // allow GCC to perform eco+ vectorization
        src_ptr32 = __builtin_assume_aligned(src_ptr32, 8);
        dst_ptr32 = __builtin_assume_aligned(dst_ptr32, 8);

        for(int i = copy_size / 4; i > 0; i--) {
             
            if((uint8_t) *(src_ptr32++) != 0) {
                *(dst_ptr32++) = white;
            }
            else
                *(dst_ptr32++) = black;
        }
        //memcpy(dst_ptr, src_ptr, copy_size);
       
        src_ptr32 += src_skip / 4;
        dst_ptr32 += dst_skip / 4;
        //src_ptr += img->pitch;
        //dst_ptr += screen.pitch;
        
    }

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

    size_t dst_skip = dst->pitch;
    size_t src_skip = src->pitch;

    size_t copy_size = width * BPP;
                            

    for(size_t i = height+1; i > 0 ; i--) {

        memcpy(dst_line_start, 
               src_line_start,
               copy_size);
        
       
       //memset(dst_line_start, 0xffffffff, copy_size);
        src_line_start += src_skip;
        dst_line_start += dst_skip;
    }
}

void imageFillRect(uint32_t color, const Rect* rect) {
    uint32_t beginx, beginy, endx, endy;

    if(rect != NULL) {
        beginx = rect->x;
        beginy = rect->y;
        endx   = rect->x + rect->w;
        endy   = rect->y + rect->h;
    }
    else {
        beginx = 0;
        beginy = 0;
        endx   = screen.w;
        endy   = screen.h;
    }

    for(size_t y = beginy; y < endy; y++) {

        uint32_t* px = (uint32_t *) (
                   screen.pix
                 + y * screen.pitch
                 + beginx * 4);
            
        for(size_t x = 0; x <= endx - beginx ; x++)
            *(px++) = color;
    }
}

void imageDraw(const Image* img, const Pos* srcpos, const Rect* dstrect) {
    imageBlit(img, &screen, srcpos, dstrect);
}


void imageBlit(const Image* restrict src, Image* restrict dst, 
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

/*

Image* alloc_image(uint32_t width, uint32_t height, uint32_t bpp) {
    Image* ret = kmalloc(sizeof(Image));

    ret->w     = width;
    ret->h     = height;
    ret->bpp   = bpp;

    assert(bpp % 8 == 0 || bpp == 1);

    if(bpp == 1) {
        // align on 1 byte
        ret->pitch = (width + 7) / 8;
    }
    else
        ret->pitch = (uint32_t)allign16(width * (bpp/8));

    ret->pix   = kmalloc(ret->pitch * height);

    return ret;
}

*/
/*
void free_image(Image* im) {
    assert(im != NULL);

    kfree(im->pix);
}
*/

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
    int32_t  w;          // BMP width in pixels
    int32_t  h;          // BMP height in pixels
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


const Image* getScreenImage(void) {
    return &screen;
}


extern uint8_t __image_pix;
/*
// assert that the checks are already performed
static Image* loadBMP_24(const struct BMPFileHeader* restrict header, const void* restrict body) {
    assert(header->bpp == 24);

    static int first = 0;
    // should use this func only once.
    // no dynamic alloc allowed
    assert(first++ == 0);

    uint32_t w = header->w;
    uint32_t h = header->h;

    static Image ret;

    ret.w     = w;
    ret.h     = h;
    ret.bpp   = 32;
    ret.pitch = (uint32_t)allign16(w * 4);
    ret.pix   = &__image_pix; 
    
    const uint8_t* srcpix  = body;
    
    const size_t bpp = 3;

    //ret = alloc_image(w,h, 32);

    size_t bpitch = ret.pitch / 4;
    uint32_t* pix32 = ret.pix;

    for(size_t y = 0; y < h; y++) {
        for(size_t x = 0; x < w; x++) {
            
            const uint32_t* src_ptr  = (const uint32_t *)(srcpix
                        + x * bpp
                        + (h-1 - y) * bpp * w);
                        /// the image is reversed along
                        /// the y axis
                        
            pix32[x + y * bpitch] = (*(const uint32_t *)src_ptr) & 0x00ffffff;  
        }
    }
    return &ret;
}
*/
/*
// assert that the checks are already performed
static Image* loadBMP_8(const struct BMPFileHeader* restrict header, const void* restrict body) {
    assert(header->bpp == 8);


    uint32_t w = header->w;
    uint32_t h = header->h;
    
    const uint8_t* srcpix  = body;
    
    Image* ret = alloc_image(w,h, 8);

    size_t bpitch = ret->pitch;
    uint32_t* pix32 = ret->pix;



    for(size_t y = 0; y < h; y++) {
        const uint8_t* src_ptr  = (srcpix + (h-1 - y) * w);
        
        for(size_t x = 0; x < w; x++) {
            /// the image is reversed along
            /// the y axis
                        
            pix32[x + y * bpitch] =  *(src_ptr++);  
        }
    }
    return ret;
}
*/


/*
Image* loadBMP(const void* restrict rawFile) {
    // the header should be at the beginning
    const struct BMPFileHeader* header = rawFile;


    if(check_BMP_header(header))
        return NULL;
    
// only handle 32bit and 8bit image
    assert(header->bpp == 8 || header->bpp == 24);

    const void* body = (uint8_t *)header + header->body_offset;

    switch(header->bpp) {
        case 24:
            return loadBMP_24(header, body);
            break;
        case 8:
            assert(0);
            //return loadBMP_8 (header, body);
        default:
            assert(0);
            __builtin_unreachable();
    }
}
*/

/*

map[] = {
(00): black black
(01): black white
(10): white black
(11): white white
}

for line
in = 

for(ptr = col; ; ptr += 2px) {
    
    in >>= 2;
    
    in' = in or 0b11

    *ptr = map+4*in'
}
*/
__attribute__((optimize("unroll-loops")))

void blitchar(const struct Image* charset,
              char c, uint32_t fg_color, uint32_t bg_color,
              uint16_t dstx, uint16_t dsty) {

    const uint64_t colormap[] = {
        ((uint64_t) bg_color << 32) | bg_color,
        ((uint64_t) bg_color << 32) | fg_color,
        ((uint64_t) fg_color << 32) | bg_color,
        ((uint64_t) fg_color << 32) | fg_color,  
    };
    
    uint16_t srcy = c * TERMINAL_FONTHEIGHT;
     

    uint64_t* dst_ptr = (uint64_t *)(screen.pix + screen.pitch * dsty + 4 * dstx);
    uint16_t  dst_skip = screen.pitch - TERMINAL_FONTWIDTH * 4;

    // 4 2-byte lines = 8*8 = 64 (1 access every 4 lines)

    // 1st line address for the selected char
    uint64_t* lines_ptr = (uint64_t*)charset->pix + srcy / sizeof(uint64_t);



    // loop over 4-line chunks

    uint64_t lines = *lines_ptr;

    // 64 bit = 8 lines
    for(size_t n_line = 8; n_line > 0; n_line--) {
        // lines are 8bit aligned
        
        for(size_t n_col2 = TERMINAL_FONTWIDTH / 2 ; n_col2 > 0 ; n_col2--) {
            uint16_t index = lines & 0b11;

            *(dst_ptr++) = colormap[index];
            lines >>= 2;
        }
        dst_ptr += dst_skip / 8;
        
        lines >>= 8 - TERMINAL_FONTWIDTH;
    }

}



__attribute__((optimize("unroll-loops")))

void blitcharX2(const struct Image* charset,
              char c, uint32_t fg_color, uint32_t bg_color,
              uint16_t dstx, uint16_t dsty) {

    const uint64_t colormap[] = {
        ((uint64_t) bg_color << 32) | bg_color,
        ((uint64_t) bg_color << 32) | fg_color,
        ((uint64_t) fg_color << 32) | bg_color,
        ((uint64_t) fg_color << 32) | fg_color,  
    };
    
    uint16_t srcy = c * TERMINAL_FONTHEIGHT;

    uint64_t* dst_ptr = (uint64_t *)(screen.pix + screen.pitch * dsty + 4 * dstx);
    uint16_t  dst_skip = 2 * screen.pitch - 2 * TERMINAL_FONTWIDTH * 4;

/// second line to modify
    uint64_t* dst_ptr2 = dst_ptr + screen.pitch / 8;

    uint64_t* lines_ptr = (uint64_t*)charset->pix + srcy / sizeof(uint64_t);
    uint64_t lines = *lines_ptr;
    for(size_t n_line = 8; n_line > 0; n_line--) {
        

        for(size_t n_col2 = TERMINAL_FONTWIDTH / 2 ; n_col2 > 0 ; n_col2--) {
            uint16_t index = lines & 0b11;

            register uint64_t pixs_val = colormap[index];
            register uint64_t pixs_val_low  = pixs_val | (pixs_val << 32);
            register uint64_t pixs_val_high = pixs_val | (pixs_val >> 32);

            *(dst_ptr++)  = pixs_val_low;
            *(dst_ptr++)  = pixs_val_high;
            *(dst_ptr2++) = pixs_val_low;
            *(dst_ptr2++) = pixs_val_high;
            // x2
            lines >>= 2;
        }
        dst_ptr +=  dst_skip / 8;
        dst_ptr2 += dst_skip / 8;
        
        
        lines >>= 8 - TERMINAL_FONTWIDTH;
    }

}

// else this func wont work 
static_assert(TERMINAL_FONTWIDTH     == 6);
static_assert(TERMINAL_FONTWIDTH % 2 == 0);
static_assert(TERMINAL_FONTHEIGHT    == 8);


static Image loadBMP_24b_1b_ret;

Image* loadBMP_24b_1b(const void* rawFile) {
    
    const struct BMPFileHeader* header = rawFile;

    static int first = 0;
    // should use this func only once.
    // no dynamic alloc allowed
    assert(first++ == 0);

    if(check_BMP_header(header))
        return NULL;


    uint32_t w = header->w;
    uint32_t h = header->h;
    
    const uint8_t* srcpix  = (const uint8_t *)header + header->body_offset;

    


    loadBMP_24b_1b_ret.w     = w;
    loadBMP_24b_1b_ret.h     = h;
    loadBMP_24b_1b_ret.bpp   = 1;
    loadBMP_24b_1b_ret.pitch = ((w+7) / 8);
    loadBMP_24b_1b_ret.pix   = &__image_pix; 

    assert(loadBMP_24b_1b_ret.pitch == 1);
    assert(w == 6);
    assert(h == 2048);

    uint8_t* pix = loadBMP_24b_1b_ret.pix;

    
    for(size_t y = 0; y < h; y++) {
        
        uint8_t byte = 0;
        
        for(size_t x = 0; x < w; x++) {
            const uint8_t* src_ptr  = (srcpix + 20 * (h-1 - y) + 3 * (w-1-x));
            byte <<= 1;
            // put 1 iif r channel > 128
            byte |= *(src_ptr) >> 7;

            

            //if((x % 8 == 0 && x != 0) || x == w-1) {
            if(x == w-1) {
                //pix[_x + y * bpitch] = byte;
            }            
        }
                pix[y] = byte;

    }

    return &loadBMP_24b_1b_ret;
}

