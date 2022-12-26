#ifndef BACKEND_H
#define BACKEND_H

#include <SDL.h>

SDL_Surface* backend_start(void);


// swap buffers and update the screen
SDL_Surface* backend_update(void);


#endif