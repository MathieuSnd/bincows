#include <unistd.h>
#include <stdio.h>
#include <SDL.h>
//#include <math.h>

int main(int argc, char** argv) {
    int ok = 1;

    double ms = 0;

    if(argc != 2)
        ok = 0;

    else {
        ms = SDL_strtod(argv[1], NULL);
        // @todo error message
    }

    //else if(!isnormal(ms))
    //    ok = 0;

    if(!ok) {
        printf("Usage: %s <seconds>\n", argv[0]);
        return 1;
    }

    usleep((uint64_t) (1000. * 1000. * ms));
}