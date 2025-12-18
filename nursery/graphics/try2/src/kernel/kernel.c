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
    gb_fill(main, color_tango_cyan());
    // gb_fill_rect(main, garea_of(0, 0, 30, 10), color_white());
    // gb_fill_rect(main, 630, 450, 40, 40, 0x993300);
    // for (int i = 0; i < 640; i += 10) {
    //     for (int j = 0; j < 480; j += 10) {
    //         color clr = color_rgb(0x33, i, j);
    //         gb_fill_rect(main, i * 2, j * 2, 18, 18, clr);
    //     }
    // }
    // for (int i = 0; i < 128; i += 7) {
    //     for (int j = 0; j < 128; j += 7) {
    //         color clr = color_rgb(255 - j, 255 - i, 0xff);
    //         gb_set_pixel(main, i,     j, clr);
    //         gb_set_pixel(main, i,     j + 1, clr);
    //         gb_set_pixel(main, i + 1, j, clr);
    //         gb_set_pixel(main, i + 1, j + 1, clr);
    //     }
    // }

    color bg = color_with_alpha(0xFF, color_tango_dark_gray());
    color shadow = color_darken(bg, 0.33);
    color letters = color_with_alpha(0xFF, color_tango_bright_white());
    
    gbuffer *r = new_gbuffer(450, 300);
    // gb_fill(r, bg);
    gb_fill_rect_rounded(r, 1, 1, 448, 298, 16, bg);
    
    // gb_copy_area(main, r, gb_size(r), gpoint_of(150, 0), gpoint_zero());
    // gb_text(r, "This is baseline 12, Geneva font", 3, 12, geneva9, color_white());
    // gb_text(r, "This is baseline 26, same font", 3, 26, geneva9, color_white());
    gb_text_demo(r, 11, 21, geneva9, shadow);
    gb_text_demo(r, 10, 20, geneva9, letters);
    gb_text_demo(r, 11, 91, geneva9_bold, shadow);
    gb_text_demo(r, 10, 90, geneva9_bold, letters);
    gb_text_demo(r, 11, 161, geneva9_mono, shadow);
    gb_text_demo(r, 10, 160, geneva9_mono, letters);
    gb_text_demo(r, 11, 231, mits7, shadow);
    gb_text_demo(r, 10, 230, mits7, letters);
    // gb_rect_border(r, 20, 40, 10, 20, color_white());

    gb_copy_area_with_alpha(main, r, gb_size(r), gpoint_of(170, 150), gpoint_zero(), 0xaa);
    gb_copy_area_with_alpha(main, r, gb_size(r), gpoint_of(10, 20), gpoint_zero(), 0xFF);

    // copy/paste offser
    // gb_copy_area(r, main, gb_size(r), gpoint_zero(), gpoint_of(10, 10));
    // gb_copy_area(main, r, gb_size(r), gpoint_of(300, 0), gpoint_zero());
    // gb_copy_area(main, r, gb_size(r), gpoint_of(300, 150), gpoint_zero());

    // gb_copy_area(main, test, gb_size(test), gpoint_of(100, 30), gpoint_zero());

    graphics_display_main_buffer();

    // kernel can never return, there's nothing to return to.
    for(;;) asm volatile("hlt");
}
