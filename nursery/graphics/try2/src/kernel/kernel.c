#include <stdint.h>
#include "../boot_info.h"
#include "memory/string.h"
#include "graphics/graphics.h"
#include "graphics/color.h"

boot_info_t global_boot_info;



void kernel_main(boot_info_t* bi) {
    // preserve boot info
    memcpy(&global_boot_info, bi, sizeof(boot_info_t));
    graphics_initialize((char *)bi->fb.fb_addr, bi->fb.width, bi->fb.height, bi->fb.pitch, bi->fb.bpp);
    graphics_fill(0x008080);
    graphics_demo(100, 100, 255, 255);

    graphics_rect(100, 380, 40, 40, color_black());
    graphics_rect(150, 380, 40, 40, 0xcc0000);
    graphics_rect(200, 380, 40, 40, 0x00cc00);
    graphics_rect(250, 380, 40, 40, 0x0000cc);
    graphics_rect(300, 380, 40, 40, color_white());

    graphics_draw_text(100, 450, "Hello world, this is your Operating System!", color_black());
    graphics_draw_text(100, 470, "Nicholas says \"Banana del Fingo!\"", color_black());

    // kernel can never return, there's nothing to return to.
    for(;;) asm volatile("hlt");
}
