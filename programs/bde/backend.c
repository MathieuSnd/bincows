#include <stdatomic.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>

#include <SDL.h>
#include "fbcopy.h"
#include "video.h"
#include "backend.h"


struct backend_data {
    SDL_Surface* backend_surf;

    // double buffering
    SDL_Surface* fbs[2];
};

static atomic_bool screen_doorbell;
static atomic_bool backend_busy;
static atomic_int  backend_frame;

static struct backend_data bd;


void* backend_thread(void* arg) {
    struct backend_data* bd = arg;

    SDL_Surface* screen = bd->backend_surf;

    size_t size = screen->pitch * screen->h;
    fbcopy_f fbcopy = get_fbcopy_f();

    // 30 FPS goal
    int min_frame_dur = 1000 * 1000 / 30; 
    
    uint64_t old_cl = time(NULL) / 1000;

    // reference buffer
    SDL_Surface* refbuf = SDL_CreateRGBSurfaceWithFormat(
                        0, screen->w, screen->h,
                         32,screen->format->format);
    while(1) {
        // update the screen if requested
        int doorbell = atomic_exchange(&screen_doorbell, 0);
        if(doorbell) {
            SDL_Surface* buffer = bd->fbs[0];

            backend_busy = 1;
            fbcopy(buffer->pixels, refbuf->pixels, screen->pixels, size);
            backend_frame++;
            backend_busy = 0;
        }

        uint64_t cl = time(NULL) / 1000;

        int64_t passed = cl - old_cl;
        if(passed < min_frame_dur) {
            // sleep for the rest of the frame
            usleep((min_frame_dur - passed));
        }
        old_cl = cl;

    }
}



SDL_Surface* backend_start(void) {
    pthread_t th;
    
    static struct dev_video info;



    int res = fetch_video_info(&info);
    if(res) {
        printf("cannot fetch video information\n");
        return NULL;
    }

    FILE* file = fopen("/mem/video", "r");
    void* framebuffer;

    assert(file);

    res = fread(&framebuffer, sizeof(framebuffer), 1, file);
    assert(res);
    assert(info.bpp == 32);

    bd.backend_surf = SDL_CreateRGBSurfaceFrom(
        framebuffer,
        info.width,
        info.height,
        32,
        info.pitch,
        0x00ff0000,
        0x0000ff00,
        0x000000ff,
        0x00000000
    );

    assert(bd.backend_surf != NULL);
    assert(bd.backend_surf->pitch != 0);

    // create double buffer
    for(int i = 0; i < 2; i++)
        bd.fbs[i] = SDL_CreateRGBSurfaceWithFormat(
            0,
            info.width,
            info.height,
            32,
            bd.backend_surf->format->format
        );

    assert(bd.fbs[0] != NULL);
    assert(bd.fbs[1] != NULL);

    pthread_create(&th, NULL, backend_thread, &bd);

    return bd.fbs[1];
}

SDL_Surface* backend_update(void) {
    // snapshot busy state
    int busy = backend_busy;
    int old_frame = backend_frame;

    SDL_Surface* tmp = bd.fbs[1];
    bd.fbs[1] = bd.fbs[0];
    bd.fbs[0] = tmp;

    screen_doorbell = 1;
    
    if(busy) 
        while(backend_frame == old_frame)
            sleep(0);

    return bd.fbs[1];
}
