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

keycode_t sdl_scancode_to_kernel_keycode(SDL_Scancode scancode) {
    switch (scancode) {
        case SDL_SCANCODE_A: return KEY_A;
        case SDL_SCANCODE_B: return KEY_B;
        case SDL_SCANCODE_C: return KEY_C;
        case SDL_SCANCODE_D: return KEY_D;
        case SDL_SCANCODE_E: return KEY_E;
        case SDL_SCANCODE_F: return KEY_F;
        case SDL_SCANCODE_G: return KEY_G;
        case SDL_SCANCODE_H: return KEY_H;
        case SDL_SCANCODE_I: return KEY_I;
        case SDL_SCANCODE_J: return KEY_J;
        case SDL_SCANCODE_K: return KEY_K;
        case SDL_SCANCODE_L: return KEY_L;
        case SDL_SCANCODE_M: return KEY_M;
        case SDL_SCANCODE_N: return KEY_N;
        case SDL_SCANCODE_O: return KEY_O;
        case SDL_SCANCODE_P: return KEY_P;
        case SDL_SCANCODE_Q: return KEY_Q;
        case SDL_SCANCODE_R: return KEY_R;
        case SDL_SCANCODE_S: return KEY_S;
        case SDL_SCANCODE_T: return KEY_T;
        case SDL_SCANCODE_U: return KEY_U;
        case SDL_SCANCODE_V: return KEY_V;
        case SDL_SCANCODE_W: return KEY_W;
        case SDL_SCANCODE_X: return KEY_X;
        case SDL_SCANCODE_Y: return KEY_Y;
        case SDL_SCANCODE_Z: return KEY_Z;
        case SDL_SCANCODE_1: return KEY_1;
        case SDL_SCANCODE_2: return KEY_2;
        case SDL_SCANCODE_3: return KEY_3;
        case SDL_SCANCODE_4: return KEY_4;
        case SDL_SCANCODE_5: return KEY_5;
        case SDL_SCANCODE_6: return KEY_6;
        case SDL_SCANCODE_7: return KEY_7;
        case SDL_SCANCODE_8: return KEY_8;
        case SDL_SCANCODE_9: return KEY_9;
        case SDL_SCANCODE_0: return KEY_0;
        case SDL_SCANCODE_RETURN: return KEY_ENTER;
        case SDL_SCANCODE_ESCAPE: return KEY_ESCAPE;
        case SDL_SCANCODE_BACKSPACE: return KEY_BACKSPACE;
        case SDL_SCANCODE_TAB: return KEY_TAB;
        case SDL_SCANCODE_SPACE: return KEY_SPACE;
        case SDL_SCANCODE_MINUS: return KEY_MINUS;
        case SDL_SCANCODE_EQUALS: return KEY_EQUAL;
        case SDL_SCANCODE_LEFTBRACKET: return KEY_LBRACKET;
        case SDL_SCANCODE_RIGHTBRACKET: return KEY_RBRACKET;
        case SDL_SCANCODE_BACKSLASH: return KEY_BACKSLASH;
        case SDL_SCANCODE_SEMICOLON: return KEY_SEMICOLON;
        case SDL_SCANCODE_APOSTROPHE: return KEY_APOSTROPHE;
        case SDL_SCANCODE_GRAVE: return KEY_BACKTICK;
        case SDL_SCANCODE_COMMA: return KEY_COMMA;
        case SDL_SCANCODE_PERIOD: return KEY_DOT;
        case SDL_SCANCODE_SLASH: return KEY_SLASH;
        case SDL_SCANCODE_CAPSLOCK: return KEY_CAPSLOCK;
        case SDL_SCANCODE_F1: return KEY_F1;
        case SDL_SCANCODE_F2: return KEY_F2;
        case SDL_SCANCODE_F3: return KEY_F3;
        case SDL_SCANCODE_F4: return KEY_F4;
        case SDL_SCANCODE_F5: return KEY_F5;
        case SDL_SCANCODE_F6: return KEY_F6;
        case SDL_SCANCODE_F7: return KEY_F7;
        case SDL_SCANCODE_F8: return KEY_F8;
        case SDL_SCANCODE_F9: return KEY_F9;
        case SDL_SCANCODE_F10: return KEY_F10;
        case SDL_SCANCODE_F11: return KEY_F11;
        case SDL_SCANCODE_F12: return KEY_F12;
        case SDL_SCANCODE_INSERT: return KEY_INSERT;
        case SDL_SCANCODE_HOME: return KEY_HOME;
        case SDL_SCANCODE_PAGEUP: return KEY_PAGEUP;
        case SDL_SCANCODE_DELETE: return KEY_DELETE;
        case SDL_SCANCODE_END: return KEY_END;
        case SDL_SCANCODE_PAGEDOWN: return KEY_PAGEDOWN;
        case SDL_SCANCODE_RIGHT: return KEY_RIGHT;
        case SDL_SCANCODE_LEFT: return KEY_LEFT;
        case SDL_SCANCODE_DOWN: return KEY_DOWN;
        case SDL_SCANCODE_UP: return KEY_UP;
        case SDL_SCANCODE_KP_DIVIDE: return KEY_KP_DIV;
        case SDL_SCANCODE_KP_MULTIPLY: return KEY_KP_MUL;
        case SDL_SCANCODE_KP_MINUS: return KEY_KP_MINUS;
        case SDL_SCANCODE_KP_PLUS: return KEY_KP_PLUS;
        case SDL_SCANCODE_KP_ENTER: return KEY_KP_ENTER;
        case SDL_SCANCODE_KP_1: return KEY_KP_1;
        case SDL_SCANCODE_KP_2: return KEY_KP_2;
        case SDL_SCANCODE_KP_3: return KEY_KP_3;
        case SDL_SCANCODE_KP_4: return KEY_KP_4;
        case SDL_SCANCODE_KP_5: return KEY_KP_5;
        case SDL_SCANCODE_KP_6: return KEY_KP_6;
        case SDL_SCANCODE_KP_7: return KEY_KP_7;
        case SDL_SCANCODE_KP_8: return KEY_KP_8;
        case SDL_SCANCODE_KP_9: return KEY_KP_9;
        case SDL_SCANCODE_KP_0: return KEY_KP_0;
        case SDL_SCANCODE_KP_PERIOD: return KEY_KP_DOT;
    }
    return KEY_NONE;
}

int convert_sdl_event(SDL_Event *sdl, event_t *ev, bool *quit) {
    memset(ev, 0, sizeof(event_t));
    
    switch (sdl->type) {
    case SDL_QUIT:
        *quit = true;
        return 1;

    case SDL_KEYUP:
    case SDL_KEYDOWN:
        keymods_t m = 0;
        
        char keyboard_ascii_from(keycode_t keycode, keymods_t modifiers);

        ev->type = EVT_KEY;
        ev->key = (key_event_t){
            .type = (sdl->type == SDL_KEYDOWN) ? KEY_PRESSED : KEY_RELEASED,
            .keycode = sdl_scancode_to_kernel_keycode(sdl->key.keysym.scancode),
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
            .wheel_delta = 0,
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
            .buttons = 0,
            .delta = vector_zero(),
            .wheel_delta = 0,
        };
        return 1;

    case SDL_MOUSEWHEEL:
        ev->type = EVT_MOUSE;
        ev->mouse = (mouse_event_t){
            .type = MOUSE_WHL_SCROLL,
            .pos = point_zero(),
            .delta = vector_zero(),
            .buttons = 0,
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
