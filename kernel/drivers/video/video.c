#include "video.h"
#include "../terminal/terminal.h"
#include "../driver.h"
#include "../../sched/shm.h"
#include "../../boot/boot_interface.h"
#include "../../fs/memfs/memfs.h"
#include "../../memory/temp.h"
#include "../../memory/paging.h"
#include "../../memory/vmap.h"
#include "../../memory/pmm.h"
#include "../../int/idt.h"

#include "../../lib/logging.h"

struct data {
    struct shm_instance* shm;

};

static 
void video_remove(driver_t* this) {
    struct data* d = this->data;
    shm_close(d->shm);
}

static
int video_install(driver_t* this) {
    struct framebuffer_dev* dev = 
            (struct framebuffer_dev *)this->device;

    if(!dev ||
        dev->dev.type != DEVICE_ID_FRAMEBUFFER ||
        dev->bpp != 32)
       return 0;

    this->remove = video_remove;
    this->name = (string_t){"Video",0};

    struct data* data = this->data = malloc(sizeof(struct data));


    size_t fb_size = dev->height * dev->pitch;

    // round to a page
    size_t fb_pages = (fb_size + 0xfff) / 0x1000;


    /////////////////////////////////////////
    //// map framebuffer in thread local ////
    ////       temp virtual memory       ////
    /////////////////////////////////////////

    assert(interrupt_enable());
    _cli();
    void* tmp_vaddr = temp_lock();

    map_pages(
        (uint64_t)dev->pix_pbase, 
        (uint64_t)tmp_vaddr,
        fb_pages ,
        PRESENT_ENTRY | PL_RW  | PL_XD 
      | PL_US // user can access the framebuffer
              // if it is mapped inside its
              // shared memory
    );


    struct shm_instance* shm = shm_create_from_kernel(fb_size, tmp_vaddr);
    
    assert(shm);

    data->shm = shm;


    uint64_t master_pd = get_master_pd(tmp_vaddr);

    unmap_master_region(tmp_vaddr);
    physfree(master_pd);


    temp_release();
    _sti();

    assert(this->remove);


    return 1;
}


typedef struct framebuffer_dev fb_t;

static driver_t* video_driver;


driver_t* video_init(const struct boot_interface* bi) {

    if(bi->framebuffer_paddr == 0)
        return NULL; // no framebuffer 

// framebuffer virtual device
    fb_t* fb_dev = 
                malloc(sizeof(fb_t));
    *fb_dev = (fb_t){
        .dev = {
            .type = DEVICE_ID_FRAMEBUFFER,
            .name = {"framebuffer",0},
            .driver = NULL
        },
        .width  = bi->framebuffer_width,
        .height = bi->framebuffer_height,
        .bpp    = bi->framebuffer_bpp,
        .pitch  = bi->framebuffer_pitch,

        .pix = (void *)FB_VADDR,
        .pix_pbase = bi->framebuffer_paddr,
    };

    log_info("screen resolution: %ux%u", 
            bi->framebuffer_width,
            bi->framebuffer_height
    );




    register_dev((struct dev *)fb_dev);

    video_driver = driver_register_and_install(
        video_install,
        (struct dev *)fb_dev
    );

    assert(video_driver);


    return driver_register_and_install(
        terminal_install,
        (struct dev *)fb_dev
    );
}


void video_create_file(char* name) {
    struct data* d = video_driver->data;

    memfs_register_file(name, d->shm->target);
}