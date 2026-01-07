#include "hosted.h"
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <stdlib.h>

static SDL_Window   *win;
static SDL_Renderer *ren;
static SDL_Texture  *tex;
static uint32_t     *fb;
static int fb_w, fb_h;

void platform_init(int w, int h) {
    fb_w = w; fb_h = h;

    SDL_Init(SDL_INIT_VIDEO);
    win = SDL_CreateWindow(
        "Kernel UI",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        w, h, 0);

    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    tex = SDL_CreateTexture(
        ren,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        w, h);

    fb = calloc(w * h, sizeof(uint32_t));
}

void platform_shutdown(void) {
    free(fb);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
}

uint32_t *platform_framebuffer(void) { return fb; }
int platform_fb_width(void)  { return fb_w; }
int platform_fb_height(void) { return fb_h; }

int platform_poll_event(platform_event_t *out) {
    SDL_Event e;
    if (!SDL_PollEvent(&e))
        return 0;

    switch (e.type) {
    case SDL_QUIT:
        out->type = PLATFORM_EVENT_QUIT;
        return 1;
    case SDL_KEYDOWN:
        out->type = PLATFORM_EVENT_KEY_DOWN;
        out->key.keycode = e.key.keysym.sym;
        return 1;
    case SDL_KEYUP:
        out->type = PLATFORM_EVENT_KEY_UP;
        out->key.keycode = e.key.keysym.sym;
        return 1;
    case SDL_MOUSEMOTION:
        out->type = PLATFORM_EVENT_MOUSE_MOVE;
        out->mouse_move.x = e.motion.x;
        out->mouse_move.y = e.motion.y;
        return 1;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        out->type = PLATFORM_EVENT_MOUSE_BUTTON;
        out->mouse_button.button = e.button.button;
        out->mouse_button.down = (e.type == SDL_MOUSEBUTTONDOWN);
        return 1;
    default:
        return 0;
    }
}

void platform_present(void) {
    SDL_UpdateTexture(tex, NULL, fb, fb_w * 4);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);
}

int main(void) {
    printf("Tada!\n");

    platform_init(1024, 768);

    int running = 1;
    platform_event_t e;

    while (running) {
        // Pump all pending events
        while (platform_poll_event(&e)) {
            if (e.type == PLATFORM_EVENT_QUIT)
                running = 0;
        }

        // For now: just present whatever is in the framebuffer
        platform_present();

        // Avoid pegging the CPU
        SDL_Delay(16); // ~60 FPS
    }

    platform_shutdown();
    return 0;
}
