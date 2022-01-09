#include <stivale2.h>

#include "early_video.h"
#include "drivers/terminal/terminal.h"
#include "memory/vmap.h"

typedef struct framebuffer_dev fd_t;

void video_init(
        const struct stivale2_struct_tag_framebuffer* framebuffer_tag
) {

// framebuffer virtual device
    fd_t* fb_dev = 
                malloc(sizeof(fd_t));
    *fb_dev = (fd_t){
        .dev = {
            .type = DEVICE_ID_FRAMEBUFFER,
            .name = {"stivale2 framebuffer",0},
            .driver = NULL
        },
        .width  = framebuffer_tag->framebuffer_width,
        .height = framebuffer_tag->framebuffer_height,
        .bpp    = framebuffer_tag->framebuffer_bpp,
        .pitch  = framebuffer_tag->framebuffer_pitch,

        .pix = (void *)MMIO_BEGIN,
    };


    register_dev((struct dev *)fb_dev);

    driver_register_and_install(
        (int (*)(struct driver*))
        terminal_install,
        (struct dev *)fb_dev
    );
}
