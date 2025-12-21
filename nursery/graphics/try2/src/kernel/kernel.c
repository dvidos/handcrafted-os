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
    // gb_fill(main, 0x008080);
    color tek_light = 0xFF0482AC;
    color nextstep_bg = 0xFF555577;
    gb_gradient_rect(main, main->area, gpoint_zero(), garea_bottom_left(main->area), nextstep_bg, color_darken(nextstep_bg, 0.12), ease_linear);



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

    color bg = color_nextstep_win_face();
    color shadow = color_darken(bg, 0.1);
    color letters = color_black();
    
    gbuffer *text_panel = new_gbuffer(450, 300);
    gb_fill_rect_rounded(text_panel, garea_of(1, 1, 400, 298), 6, bg);
    gb_rect_border_rounded(text_panel, garea_of(1, 1, 400, 298), 6, 1, color_black());
    
    // gb_copy_area(main, r, gb_size(r), gpoint_of(150, 0), gpoint_zero());
    // gb_text(r, "This is baseline 12, Geneva font", 3, 12, geneva9, color_white());
    // gb_text(r, "This is baseline 26, same font", 3, 26, geneva9, color_white());
    gb_text_demo(text_panel, 11, 21, geneva9, shadow);
    gb_text_demo(text_panel, 10, 20, geneva9, letters);
    gb_text_demo(text_panel, 11, 91, geneva9_bold, shadow);
    gb_text_demo(text_panel, 10, 90, geneva9_bold, letters);
    gb_text_demo(text_panel, 11, 161, geneva9_mono, shadow);
    gb_text_demo(text_panel, 10, 160, geneva9_mono, letters);
    gb_text_demo(text_panel, 11, 231, mits7, shadow);
    gb_text_demo(text_panel, 10, 230, mits7, letters);


    gbuffer *text_panel_shadow = new_gbuffer(500, 350);
    gb_drop_shadow(text_panel_shadow, text_panel, shadow_params_of(color_black(), 0x33, 3, 3, 2));
    gb_copy_area_with_alpha(main, text_panel_shadow, text_panel_shadow->area.size, gpoint_of(20, 120), gpoint_zero(), 0xFF);

    // gb_copy_area_with_alpha(main, text_panel, text_panel->area.size, gpoint_of(170, 150), gpoint_zero(), 0x88);
    gb_copy_area_with_alpha(main, text_panel, text_panel->area.size, gpoint_of(20, 120), gpoint_zero(), 0xFF);
    // gb_blur(main, garea_of(250, 120, 300, 150), 2, 0);
    // // gb_rect_border(main, 250, 120, 300, 150, color_black());
    // // let's give a semitransparent white buffer on top...
    // gbuffer *glass = new_gbuffer(300, 150);
    // gb_fill(glass, color_black());
    // gb_copy_area_with_alpha(main, glass, gsize_of(300, 150), gpoint_of(250, 120), gpoint_zero(), 0x22);

    // copy/paste offser
    // gb_copy_area(r, main, gb_size(r), gpoint_zero(), gpoint_of(10, 10));
    // gb_copy_area(main, r, gb_size(r), gpoint_of(300, 0), gpoint_zero());
    // gb_copy_area(main, r, gb_size(r), gpoint_of(300, 150), gpoint_zero());

    // gb_copy_area(main, test, gb_size(test), gpoint_of(100, 30), gpoint_zero());

    gbuffer *rr = new_gbuffer(250, 250);
    gb_rect_border_rounded(rr, garea_of(0, 0, 220, 150), 16, 2, color_tango_black());
    
    gbuffer *rr_shadow = new_gbuffer(350, 350);
    gb_drop_shadow(rr_shadow, rr, shadow_params_of(0xff000000, 0x55, 4, 3, 2));
    
    gb_copy_area_with_alpha(main, rr_shadow, rr_shadow->area.size, gpoint_of(220, 100), gpoint_zero(), 0xFF);
    gb_copy_area_with_alpha(main, rr, rr->area.size, gpoint_of(220, 100), gpoint_zero(), 0xFF);


    color c1 = color_gray_of(0xcc);
    color c2 = color_gray_of(0x33);
    gb_gradient_rect(main, garea_of( 20, 20, 50, 50), gpoint_of(0, 0), gpoint_of(0, 50), c1, c2, ease_linear);
    gb_gradient_rect(main, garea_of( 80, 20, 50, 50), gpoint_of(0, 0), gpoint_of(0, 50), c1, c2, ease_in_quad);
    gb_gradient_rect(main, garea_of(140, 20, 50, 50), gpoint_of(0, 0), gpoint_of(0, 50), c1, c2, ease_out_quad);
    gb_gradient_rect(main, garea_of(200, 20, 50, 50), gpoint_of(0, 0), gpoint_of(0, 50), c1, c2, ease_in_out);
    gb_gradient_rect(main, garea_of(260, 20, 50, 50), gpoint_of(0, 0), gpoint_of(0, 50), c1, c2, ease_bevel);
    gb_gradient_rect(main, garea_of(320, 20, 50, 50), gpoint_of(0, 0), gpoint_of(0, 50), c1, c2, ease_bevel_highlight);
    gb_gradient_rect(main, garea_of(380, 20, 50, 50), gpoint_of(0, 0), gpoint_of(0, 50), c1, c2, ease_piecewise);

    // yeah, it'd be nice to have "center alignment"
    gb_text(main, "linear",     20, 82, mits7, color_gray_of(0xcc));
    gb_text(main, "in_quad",    80, 82, mits7, color_gray_of(0xcc));
    gb_text(main, "out_quad",  140, 82, mits7, color_gray_of(0xcc));
    gb_text(main, "in_out",    200, 82, mits7, color_gray_of(0xcc));
    gb_text(main, "bevel",     260, 82, mits7, color_gray_of(0xcc));
    gb_text(main, "bevel_hl",  320, 82, mits7, color_gray_of(0xcc));
    gb_text(main, "piecewise", 380, 82, mits7, color_gray_of(0xcc));




    graphics_display_main_buffer();

    // kernel can never return, there's nothing to return to.
    for(;;) asm volatile("hlt");
}
