#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <stdlib.h>
#include "../../../boot_info.h"
#include "../../concepts/events.h"



static SDL_Window   *win;
static SDL_Renderer *ren;
static SDL_Texture  *tex;
static uint32_t     *fb;
static int fb_w, fb_h;



void sdl_init(int w, int h) {
    fb_w = w; fb_h = h;

    SDL_Init(SDL_INIT_VIDEO);
    win = SDL_CreateWindow(
        "Kernel UI",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        w, h,
        SDL_WINDOW_SHOWN);

    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    tex = SDL_CreateTexture(
        ren,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        w, h);

    fb = calloc(w * h, sizeof(uint32_t));
}

void sdl_shutdown(void) {
    free(fb);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
}

int convert_sdl_event(SDL_Event *sdl, event_t *ev, bool *quit) {
    switch (sdl->type) {
    case SDL_QUIT:
        *quit = true;
        return 1;

    case SDL_KEYUP:
    case SDL_KEYDOWN:
        keymods_t m = 0;
        
        keycode_t keyboard_keycode_from(int scancode, int is_e0);
        char keyboard_ascii_from(keycode_t keycode, keymods_t modifiers);

        ev->type = EVT_KEY;
        ev->key = (key_event_t){
            .type = (sdl->type == SDL_KEYDOWN) ? KEY_PRESSED : KEY_RELEASED,
            .keycode = keyboard_keycode_from(sdl->key.keysym.scancode, 0),
        };
        SDL_Keymod sm = SDL_GetModState();
        if (sm & KMOD_CTRL)  ev->key.keymods |= KEY_CTRL;
        if (sm & KMOD_ALT)   ev->key.keymods |= KEY_ALT;
        if (sm & KMOD_SHIFT) ev->key.keymods |= KEY_SHIFT;
        if (sm & KMOD_GUI)   ev->key.keymods |= KEY_SUPER;
        ev->key.ascii = keyboard_ascii_from(ev->key.keycode, ev->key.keymods);
        return 1;

    case SDL_MOUSEMOTION:
        ev->type = EVT_MOUSE;
        ev->mouse = (mouse_event_t){
            .type = MOUSE_MOVED,
            .buttons = sdl->motion.state,
            .delta = vector_of(sdl->motion.xrel, sdl->motion.yrel),
            .pos = point_of(sdl->motion.x, sdl->motion.y),
        };
        return 1;

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        // sdl->button.button is which button was pressed/released
        // sdl->button.state is either SDL_PRESSED or ::SDL_RELEASED
        mouse_event_type met;
        if      (sdl->button.button == SDL_BUTTON_LEFT   && sdl->button.state == SDL_PRESSED)  met = MOUSE_LBTN_DOWN;
        else if (sdl->button.button == SDL_BUTTON_LEFT   && sdl->button.state == SDL_RELEASED) met = MOUSE_LBTN_UP;
        else if (sdl->button.button == SDL_BUTTON_MIDDLE && sdl->button.state == SDL_PRESSED)  met = MOUSE_MBTN_DOWN;
        else if (sdl->button.button == SDL_BUTTON_MIDDLE && sdl->button.state == SDL_RELEASED) met = MOUSE_MBTN_UP;
        else if (sdl->button.button == SDL_BUTTON_RIGHT  && sdl->button.state == SDL_PRESSED)  met = MOUSE_RBTN_DOWN;
        else if (sdl->button.button == SDL_BUTTON_RIGHT  && sdl->button.state == SDL_RELEASED) met = MOUSE_RBTN_UP;
        ev->type = EVT_MOUSE;
        ev->mouse = (mouse_event_t){
            .type = met,
            .pos = point_of(sdl->button.x, sdl->button.y),
        };
        return 1;

    case SDL_MOUSEWHEEL:
        ev->type = EVT_MOUSE;
        ev->mouse = (mouse_event_t){
            .type = MOUSE_WHL_SCROLL,
            .wheel_delta = sdl->wheel.y
        };
        return 1;

    default:
        return 0;
    }
}

void sdl_present(void) {
    SDL_UpdateTexture(tex, NULL, fb, fb_w * 4);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);
}

bool hosted_get_event(event_t *ev) {
    bool quit = false;
    bool got_event = false;
    SDL_Event sdl;

    while (SDL_PollEvent(&sdl)) {
        if (convert_sdl_event(&sdl, ev, &quit)) {
            got_event = true;
            break;
        }
    }

    sdl_present(); // For now: just present whatever is in the framebuffer
    SDL_Delay(16); // ~60 FPS, Avoid pegging the CPU
    
    if (quit) {
        sdl_shutdown();
        exit(0);
    }
    
    return got_event;
}



// this is the entry point in HOSTED_ENV (debuggable executable)
int main(void) {
    sdl_init(1024, 768);
    
    boot_info_t boot_info;
    memset(&boot_info, 0, sizeof(boot_info_t));
    boot_info.fb.fb_addr = (uint64_t)fb;
    boot_info.fb.width = fb_w;
    boot_info.fb.height = fb_h;
    boot_info.fb.pitch = fb_w * 4;
    boot_info.fb.bpp = 32;
    
    extern void kernel_main(boot_info_t* bi);
    kernel_main(&boot_info);
}
