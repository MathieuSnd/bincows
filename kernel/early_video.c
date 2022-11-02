#include "early_video.h"
#include "drivers/terminal/terminal.h"
#include "memory/vmap.h"
#include "boot/boot_interface.h"

typedef struct framebuffer_dev fd_t;

driver_t* video_init(
        const struct boot_interface* bi
) {

    if(bi->framebuffer_paddr == 0)
        return NULL; // no framebuffer 

// framebuffer virtual device
    fd_t* fb_dev = 
                malloc(sizeof(fd_t));
    *fb_dev = (fd_t){
        .dev = {
            .type = DEVICE_ID_FRAMEBUFFER,
            .name = {"stivale2 framebuffer",0},
            .driver = NULL
        },
        .width  = bi->framebuffer_width,
        .height = bi->framebuffer_height,
        .bpp    = bi->framebuffer_bpp,
        .pitch  = bi->framebuffer_pitch,

        .pix = (void *)MMIO_BEGIN,
    };

    log_info("screen resolution: %ux%u", 
            bi->framebuffer_width,
            bi->framebuffer_height
    );




    register_dev((struct dev *)fb_dev);

    return driver_register_and_install(
        (int (*)(struct driver*))
        terminal_install,
        (struct dev *)fb_dev
    );
}
