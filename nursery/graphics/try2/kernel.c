#include <stdint.h>
#include "boot_info.h"

boot_info_t global_boot_info;
boot_info_t *gbi = &global_boot_info;

void memcpy(void *dest, void *src, int len) {
    while (len-- > 0) {
        *(unsigned char *)dest = *(unsigned char *)src;
        dest += 1;
        src += 1;
    }
}

static void graphics_demo() {
    // demonstration!
    // uint8_t *fb = (uint8_t *)gbi->fb.fb_addr;
    // for (int y = 0; y < 255; y++) {
    //     for (int x = 0; x < 255; x++) {
    //         uint8_t *pix_start = fb + y * gbi->fb.pitch + x * 3;
    //         pix_start[0] = x & 0xFF; // blue
    //         pix_start[1] = y & 0xFF; // green
    //         pix_start[2] = (y+x) & 0xFF; // red
    //     }
    // }

    for (int i = 0; i < 4096; i++) {
        ((uint8_t *)gbi->fb.fb_addr)[i] = i & 0xFF;
    }
    for(;;) asm("hlt");

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
    // for(;;) asm volatile("hlt");

    memcpy(&global_boot_info, bi, sizeof(boot_info_t));

    graphics_demo();

    // kernel can never return, there's nothing to return to.
    for(;;) asm volatile("hlt");
}
