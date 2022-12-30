#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <SDL.h>

#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/events.h>

extern "C" {
#include "fbcopy.h"
#include "video.h"
#include "backend.h"
}

#include "factory.hxx"

#define OPCODE_PROCESS_EVENTS 1

struct extout_cmd {
    uint32_t opcode;
    uint8_t cmd[32];
};

static void enable_events(int en) {
    struct extout_cmd cmd = {
        OPCODE_PROCESS_EVENTS,
        !en
    };

    write(2, &cmd, sizeof(cmd));
}

static void disable_events(void) {
    enable_events(0);
} 

#include "bde_cmd.h"


/*
    structure for inter-process communication
    this process receives commands through the command pipe
    and sends responses through the response pipe
*/
struct ipc_t {
    FILE* cmd_f;
    char* shared_name;
    void* shared_base;
    

    void close() {
        fclose(cmd_f);
        shm_unlink(shared_name);
        free(shared_name);
    }

    int read(bde_cmd& cmd) {
        return fread(&cmd, 1, sizeof(cmd), cmd_f) == sizeof(cmd);
    }
    int write(bde_resp& resp) {
        
    }


    ipc_t() {
        /*
            target FD layout:
            - 0 - stdin  /dev/ps2
            - 1 - stdout /dev/term
            - 2 - extout pipe -> init
            - 3 - window command pipe write
            - 4 - bde shared space
            -------------------------------
            - ? - window command pipe read
        */
       static int i = 0;
       i++;
       shared_name = (char*)malloc(128);
       snprintf(shared_name, 128, "/bde__%u", i);

        int pipe_ends[2];

        pipe(pipe_ends);
        assert(pipe_ends[0] == 3);
        assert(pipe_ends[1] == 4);




        int cmd_pipe_read = dup(3);
        dup2(4, 3);
        ::close(4);

        int shared_fd = shm_open(shared_name, O_CREAT, static_cast<mode_t>(0));         
        assert(shared_fd == 4);

        shared_base = mmap(NULL, 0, 0, 0, 0, 0);
        int r = ftruncate(shared_fd, sizeof(struct bde_shared_space));
        

        cmd_f  = fdopen(cmd_pipe_read,   "r");
    }

    void clean_lower_fds(void) {
        ::close(3);
        ::close(4);
    }
};

static int init_fd_layout(void) {
    stderr = stdout;

    dup2(3, 2);
    close(3);
}

struct window_t {
    int id;

    int x, y;
    int w, h;


    window_t(int id, int x, int y, SDL_Surface* s):
        id(id), x(x), y(y), w(s->w), h(s->h), surface(s)
    {
        assert(s);
    }

    SDL_Surface* surface;

    void blit(SDL_Surface* screen, SDL_Rect const& rect) {
        int dstxend = SDL_min(x + w, rect.x + rect.w);
        int dstyend = SDL_min(y + h, rect.y + rect.h);

        SDL_Rect dstrect = {
            .x = SDL_max(rect.x, x),
            .y = SDL_max(rect.y, y),
        };
        
        dstrect.w = dstxend - dstrect.x;
        dstrect.h = dstyend - dstrect.y;


        SDL_Rect srcrect = {
            .x = dstrect.x - x,
            .y = dstrect.x - y,
            dstrect.w,
            dstrect.h,
        };


        assert(surface);

        int res = SDL_LowerBlit(surface, &srcrect, screen, &dstrect);

        if(res < 0) {
            printf("window_t::blit: SDL erorr %s\n", SDL_GetError());
            exit(1);
        }
    }


    void close() {
        assert(surface);
        SDL_FreeSurface(surface);
    }
};


struct process_t {

    process_t(int id, ipc_t ipc):
        window_ids(), id(id), ipc(ipc)
     {

    }

    factory<factory_unclosable_item<int>> window_ids;

    // pid
    int id;

    ipc_t ipc;

    void close() {
        ipc.close();
    }
};

namespace {
    factory<process_t> processes;
    factory<window_t> windows;

    static
    void blit_windows(SDL_Surface* screen, SDL_Rect& rect) {
        for(auto& win: windows) {
            win.blit(screen, rect);
        }
    }

    

    static
    void blit_windows(SDL_Surface* screen) {
        SDL_Rect rect = {
            .x = 0,
            .y = 0,
            .w = screen->w,
            .h = screen->h,
        };
        blit_windows(screen, rect);
    }
};



static void close_window(int wid) {
    // @todo
}


static void handle_cmd(
                bde_cmd const& cmd, 
                bde_resp     & resp) 
{
    // @todo
}


static
void* cmd_pipe_thread(void* arg) {
    process_t* process = (process_t*)arg;
    struct bde_cmd cmd;
    struct bde_resp resp;

    while(1) {
        if(!process->ipc.read(cmd))
            break;

        handle_cmd(cmd, resp);

        if(!process->ipc.write(resp))
            break;
    }


    // free everything
    processes.remove(process);
}




static void execute_gui_program(const char* path, ipc_t* ipc) {
    const char* const cmdline[] = {path, NULL};

    int pid = forkexec(cmdline, 0x1f);

    if(pid < 0) {
        // couldn't execute
        printf("couldn't execute %s.\n", path);
        exit(1);
    }

    *ipc = ipc_t();
}


extern "C"
int main(int argc, char** argv) {

    init_fd_layout();
    ipc_t ipc{};
    
    processes = factory<process_t>();
    windows   = factory<window_t>();

    
    enable_events(1);

    SDL_Init(SDL_INIT_NOPARACHUTE);

    atexit(disable_events);

    SDL_Surface* screen = backend_start();

    if(!screen)
        return 1;

    fast_SDL_Surface* bg = load_fast_bmp("/etc/bde/wallpaper.bmp", screen);
    assert(bg);

    // add the background as a window
    windows.emplace_back(0, 0,0, bg->surface);


    SDL_Rect rect = {0,0,0,0};
    FILE* event_f = fopen(SYS_EVENT_FILE, "r");

    assert(event_f);

    fast_SDL_Surface* cursor = load_fast_bmp("/etc/bde/cursor.bmp", screen);
    SDL_SetColorKey(cursor->surface, SDL_TRUE, 0x00ff00);
    
    float cursor_x = screen->w / 2.f;
    float cursor_y = screen->h / 2.f;

    float cursor_sensibility = 1.2;

    // initial blit
    blit_windows(screen);
    screen = backend_update();
    blit_windows(screen);
    screen = backend_update();
    

    int redraw;
    SDL_Rect update_rect;
    int mouse_update = 0;

    while(1) {
        struct sys_event ev;
        int r = fread(&ev, 1, sizeof(struct sys_event), event_f);
        if(r != sizeof(struct sys_event)) {
            assert(0);
            return 1;
        }

        if(ev.type == MOUSE_MOVE) {
            int old_cursor_x = cursor_x;
            int old_cursor_y = cursor_y;

            cursor_x += cursor_sensibility * ev.mouse.xrel;
            cursor_y += cursor_sensibility * ev.mouse.yrel;

            if(cursor_x < 0)
                cursor_x = 0;
            else if(cursor_x > screen->w-1)
                cursor_x = screen->w-1;

            if(cursor_y < 0)
                cursor_y = 0;
            else if(cursor_y > screen->h-1)
                cursor_y = screen->h-1;


            redraw = 1;
            update_rect = (SDL_Rect) {
                .x = SDL_min((int)cursor_x, old_cursor_x),
                .y = SDL_min((int)cursor_y, old_cursor_y),
                .w = SDL_abs((int)cursor_x - old_cursor_x),
                .h = SDL_abs((int)cursor_y - old_cursor_y),
            };
            mouse_update = 1;
        }

        if(redraw) {
            redraw = 0;
            //blit_windows(screen, update_rect);
            blit_windows(screen);
        }

        if(mouse_update) {
            SDL_Rect cursor_pos = {
                .x = (int)cursor_x,
                .y = (int)cursor_y,
            };

            SDL_BlitSurface(cursor->surface, NULL, screen, &cursor_pos);
        }

        if(redraw || mouse_update)
            screen = backend_update();

    }
}
