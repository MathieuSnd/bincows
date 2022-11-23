#ifndef DRIVER_VIDEO_H
#define DRIVER_VIDEO_H

#define DEVICE_ID_FRAMEBUFFER (0xfa30bffe)

#include <stdint.h>
#include "../dev.h"

struct driver;



// virtual device
struct framebuffer_dev {
    struct dev dev;

    void*    pix;
    uint64_t pix_pbase;
    unsigned width, height;
    unsigned pitch;
    unsigned bpp;
};

struct dev_video {
    unsigned width, height;
    unsigned pitch;
    unsigned bpp;
};

struct boot_interface;

// return the terminal driver
struct driver* video_init(const struct boot_interface *);


// create a file on /mem/`name
void video_create_file(char* name);





#endif//DRIVER_VIDEO