#include <stdint.h>
#include "../boot_info.h"
#include "memory/string.h"

boot_info_t global_boot_info;


static void gf_fill(uint32_t color) {
    framebuffer_info_t *fb = &global_boot_info.fb;
    uint8_t red   = (color >> 16) & 0xFF;
    uint8_t green = (color >> 8) & 0xFF;
    uint8_t blue  = (color >> 0) & 0xFF;

    for (int y = 0; y < fb->height; y++) {
        for (int x = 0; x < fb->width; x++) {
            uint8_t *pix_start = ((uint8_t *)fb->fb_addr) + y * fb->pitch + x * 3;
            pix_start[2] = red;
            pix_start[1] = green;
            pix_start[0] = blue;
        }
    }
}

static void graphics_demo() {
    // demonstration!
    framebuffer_info_t *fb = &global_boot_info.fb;
    for (int y = 0; y < 255; y++) {
        for (int x = 0; x < 255; x++) {
            uint8_t *pix_start = ((uint8_t *)fb->fb_addr) + (y+100) * fb->pitch + (x+100) * 3;
            pix_start[0] = y & 0xFF; // blue
            pix_start[1] = x & 0xFF; // green
            pix_start[2] = (y^x) & 0xFF; // red
        }
    }

    // for (int i = 0; i < 64 * 4096; i++) {
    //     ((uint8_t *)gbi->fb.fb_addr)[i] = i & 0xFF;
    //     ((uint8_t *)gbi->fb.fb_addr)[i] = i & 0xFF;
    //     ((uint8_t *)gbi->fb.fb_addr)[i] = i & 0xFF;
    // }

    // rect_filled(260, 0, 280, 20, 0x0000cc);
    // rect_filled(280, 0, 300, 20, 0x00cc00);
    // rect_filled(300, 0, 320, 20, 0xcc0000);

    // rect_border(260, 0, 280, 20, 0xffffff);
    // rect_border(280, 0, 300, 20, 0xffffff);
    // rect_border(300, 0, 320, 20, 0xffffff);

    // rect_filled(260, 20, 280, 40, 0x444444);
    // rect_filled(280, 20, 300, 40, 0x888888);
    // rect_filled(300, 20, 320, 40, 0xcccccc);

    // rect_border(260, 20, 280, 40, 0xffffff);
    // rect_border(280, 20, 300, 40, 0xffffff);
    // rect_border(300, 20, 320, 40, 0xffffff);
}

void kernel_main(boot_info_t* bi) {
    // preserve boot info
    memcpy(&global_boot_info, bi, sizeof(boot_info_t));


    gf_fill(0x008080);
    graphics_demo();

    // kernel can never return, there's nothing to return to.
    for(;;) asm volatile("hlt");
}
