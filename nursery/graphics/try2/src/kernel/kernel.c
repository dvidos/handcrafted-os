#include <stdint.h>
#include "../boot_info.h"
#include "memory/string.h"
#include "graphics/graphics.h"
#include "graphics/gbuffer.h"
#include "graphics/color.h"

boot_info_t global_boot_info;



void kernel_main(boot_info_t* bi) {
    // preserve boot info
    memcpy(&global_boot_info, bi, sizeof(boot_info_t));
    graphics_initialize((char *)bi->fb.fb_addr, bi->fb.width, bi->fb.height, bi->fb.pitch, bi->fb.bpp);

    // graphics_fill(0xffcc00);
    // graphics_demo(100, 100, 255, 255);
    // graphics_rect(100, 380, 40, 40, color_black());
    // graphics_rect(150, 380, 40, 40, 0xcc0000);
    // graphics_rect(200, 380, 40, 40, 0x00cc00);
    // graphics_rect(250, 380, 40, 40, 0x0000cc);
    // graphics_rect(300, 380, 40, 40, color_white());
    // graphics_draw_8x16_text(10,  20, "Hello world, this is your Operating System!", mits7, color_black());
    // graphics_draw_8x16_text(10,  30, "Nicholas says \"Banana del Fingo!\"", mits7, color_black());
    // graphics_draw_8x16_demo(10,  60, geneva9, color_black());
    // graphics_draw_8x16_demo(10, 130, mits7,   color_black());

    gbuffer *main = graphics_get_main_buffer();
    main->buffer[0] = 0xff;
    main->buffer[1] = 0xff;
    main->buffer[2] = 0xff;
    for (int i = 0; i < 256; i++) {
        for (int j = 0; j < 256; j++) {
            gb_set_pixel(main, i, j, (0x008800 | (i << 16) | j));
        }
    }
    // gb_fill_rect(main, garea_of(0, 0, 30, 10), color_white());
    // gb_fill(main, 0x708090);

    // gbuffer *test = new_gbuffer(700, 100, 150, 32);
    // gb_fill(test, 0x999999);
    // gb_rect_border(test, garea_of(0, 0, test->width, test->height), color_black());
    // // // gb_fill_rect(test, garea_of(10, 10, 30, 30), 0xffcc00);
    // // // gb_rect_border(test, garea_of(10, 10, 30, 30), color_black());
    // gb_copy_area(main, test, gb_size(test), gpoint_of(100, 30), gpoint_zero());

    graphics_display_main_buffer();

    // kernel can never return, there's nothing to return to.
    for(;;) asm volatile("hlt");
}
