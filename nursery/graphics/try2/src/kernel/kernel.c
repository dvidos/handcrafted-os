#include <stdint.h>
#include "../boot_info.h"
#include "cpu/ports.h"
#include "cpu/pic.h"
#include "cpu/idt.h"
#include "cpu/timer.h"
#include "memory/string.h"
#include "memory/sprintf.h"
#include "aux/logger.h"
#include "aux/events.h"
#include "graphics/graphics.h"
#include "graphics/gbuffer.h"
#include "graphics/color.h"
#include "devs/mouse.h"
#include "devs/keyboard.h"

boot_info_t global_boot_info;




void rectangles_borders_demo() {
    gbuffer *main = graphics_get_main_buffer();
    gb_fill(main, 0x008080);
    gb_text(main, "Rectangles & borders demo", 5, 13, geneva9, color_white());

    gpoint pos = gpoint_of(20, 40);
    gsize size = gsize_of(150, 130);

    // color_params win = color_params_solid(color_nextstep_win_face());
    color_params win = color_params_gradient(
        color_gray_of(0xdd),
        color_gray_of(0x88),
        gpoint_zero(), gpoint_of(size.width / 2, size.height),
        ease_linear);

    color_params win_dark = color_params_solid(color_nextstep_win_shadow());
    gbuffer *work = new_gbuffer(main->area.size.width, main->area.size.height);

    int thicknesses[] = { 1, 2, 5 };
    int radii[] = { 0, 8, 16 };

    for (int i = 0; i < 3; i++) {
        gb_clear(work);

        gb_rect(work, garea_with(gpoint_move(pos, 18, 0), size), win_dark, radii[i]);
        gb_rect(work, garea_with(gpoint_move(pos, 0, 12), size), win, radii[i]);
        gb_border(work, garea_with(gpoint_move(pos, 12, 25), size), radii[i], thicknesses[i], color_black());
        pos = gpoint_move(pos, size.width + 40, 0);
    
        gb_copy_area_with_alpha(main, work, work->area.size, gpoint_zero(), gpoint_zero(), 0xFF);
    }

    graphics_display_main_buffer();
}

void blend_demo() {
    gbuffer *main = graphics_get_main_buffer();
    gb_fill(main, 0x444444);
    gb_text(main, "Horiz: red alpha, vert: blue alpha, blend(bottom=red, top=blue), alpha 0..255:16", 5, 13, geneva9, color_white());
    
    gbuffer *tile = new_gbuffer(30, 24);
    for (int red_alpha = 0; red_alpha < 16; red_alpha++) {
        for (int blue_alpha = 0; blue_alpha < 16; blue_alpha++) {
            color top = color_with_alpha(red_alpha* (256/16), 0x0000cc);
            color bottom = color_with_alpha(blue_alpha * (256/16), 0xcc0000);
            color blended = color_blend(bottom, top);
            gb_fill(tile, blended);

            gpoint pos = gpoint_of(5 + red_alpha * (tile->area.size.width + 3), 25 + blue_alpha * (tile->area.size.height + 3));
            gb_copy_area_with_alpha(main, tile, tile->area.size, pos, gpoint_zero(), 0xFF);
        }
    }

    graphics_display_main_buffer();
}

void fonts_demo() {
    gbuffer *main = graphics_get_main_buffer();
    gb_fill(main, 0x008080);

    color bg = color_nextstep_win_face();
    color shadow = color_darken(bg, 0.15);
    color letters = color_black();
    
    gbuffer *text_panel = new_gbuffer(450, 320);
    gb_fill(text_panel, bg);
    gb_text_demo(text_panel, 11, 21, geneva9, shadow);
    gb_text_demo(text_panel, 10, 20, geneva9, letters);
    gb_text_demo(text_panel, 11, 101, geneva9_bold, shadow);
    gb_text_demo(text_panel, 10, 100, geneva9_bold, letters);
    gb_text_demo(text_panel, 11, 181, geneva9_mono, shadow);
    gb_text_demo(text_panel, 10, 180, geneva9_mono, letters);
    gb_text_demo(text_panel, 11, 261, mits7, shadow);
    gb_text_demo(text_panel, 10, 260, mits7, letters);

    gb_copy_area(main, text_panel, text_panel->area.size, gpoint_of(30, 30), gpoint_zero());
    graphics_display_main_buffer();
}

void gradient_demo() {
    gbuffer *main = graphics_get_main_buffer();
    gb_fill(main, 0x008080);

    gsize tile_size = gsize_of(70, 70);
    gpoint gp1 = gpoint_zero();
    gpoint gp2 = gpoint_of(0, tile_size.width);
    gpoint pos = gpoint_of(20, 20);
    color c1 = color_gray_of(0xcc);
    color c2 = color_gray_of(0x66);
    color text_color = color_gray_of(0x11);
    color border_color = color_gray_of(0x33);

    ease_function *eases[] = {
        ease_linear,
        ease_in_quad,
        ease_out_quad,
        ease_in_out,
        ease_bevel,
        ease_bevel_highlight,
        ease_piecewise
    };

    const char *names[] = {
        "linear",
        "in_quad",
        "out_quad",
        "in_out",
        "bevel",
        "bevel_highlight",
        "piecewise"
    };

    // yeah, it'd be nice to have "center alignment"
    pos = gpoint_of(20, 20);
    for (int i = 0; i < sizeof(names)/sizeof(names[0]); i++) {
        gb_text(main, names[i], pos.x, pos.y, mits7, text_color); 
        pos.x += tile_size.width + 10;
    }

    // horiz gradient
    pos = gpoint_of(20, pos.y + 10);
    gp2.x = 0;
    for (int i = 0; i < sizeof(eases)/sizeof(eases[0]); i++) {
        gb_rect(main, garea_with(pos, tile_size), color_params_gradient(c1, c2, gp1, gp2, eases[i]), 0);
        gb_border(main, garea_with(pos, tile_size), 0, 1, border_color);
        pos.x += tile_size.width + 10;
    }

    // slightly slanted towards diag
    pos = gpoint_of(20, pos.y + tile_size.height + 10);
    gp2.x = tile_size.width / 3;
    for (int i = 0; i < sizeof(eases)/sizeof(eases[0]); i++) {
        gb_rect(main, garea_with(pos, tile_size), color_params_gradient(c1, c2, gp1, gp2, eases[i]), 0);
        gb_border(main, garea_with(pos, tile_size), 0, 1, border_color);
        pos.x += tile_size.width + 10;
    }

    // 45 deg diag
    pos = gpoint_of(20, pos.y + tile_size.height + 10);
    gp2.x = tile_size.width;
    for (int i = 0; i < sizeof(eases)/sizeof(eases[0]); i++) {
        gb_rect(main, garea_with(pos, tile_size), color_params_gradient(c1, c2, gp1, gp2, eases[i]), 0);
        gb_border(main, garea_with(pos, tile_size), 0, 1, border_color);
        pos.x += tile_size.width + 10;
    }

    // almost horizontal
    pos = gpoint_of(20, pos.y + tile_size.height + 10);
    gp2 = gpoint_of(tile_size.width, tile_size.height / 4);
    for (int i = 0; i < sizeof(eases)/sizeof(eases[0]); i++) {
        gb_rect(main, garea_with(pos, tile_size), color_params_gradient(c1, c2, gp1, gp2, eases[i]), 0);
        gb_border(main, garea_with(pos, tile_size), 0, 1, border_color);
        pos.x += tile_size.width + 10;
    }

    graphics_display_main_buffer();
}


void blur_demo() {
    char buffer[64];

    gbuffer *main = graphics_get_main_buffer();
    gb_fill(main, 0xFF007777);
    gb_text(main, "Blurring demo (fast boxing algorithm x3)", 5, 13, geneva9, color_white());
    int tile_side = 80;
    gsize tile_size = gsize_of(tile_side, tile_side);

    int sq_size[] = { 3, 7, 15, 30 };
    int blur_radii[] = { 0, 1, 2, 4, 8, 16 };

    for (int r = 0; r < sizeof(blur_radii)/sizeof(blur_radii[0]); r++) {
        gpoint p = gpoint_of(12 + r * (tile_side + 10), 30);
        sprintfn(buffer, sizeof(buffer), "r=%d", blur_radii[r]);
        gb_text(main, buffer, p.x, p.y, mits7, color_gray_of(0xcc));
    }

    for (int s = 0; s < sizeof(sq_size)/sizeof(sq_size[0]); s++) {
        for (int r = 0; r < sizeof(blur_radii)/sizeof(blur_radii[0]); r++) {
            gpoint p = gpoint_of(10 + r * (tile_side + 10), 40 + s * (tile_side + 10));
            int offset = (tile_side/2) - sq_size[s] / 2;

            gb_rect(main, garea_with(gpoint_move(p, offset + 3, offset - 3), gsize_of(sq_size[s], sq_size[s])), color_params_solid(0xcccc00), 0);
            gb_rect(main, garea_with(gpoint_move(p, offset, offset), gsize_of(sq_size[s], sq_size[s])), color_params_solid(0x0000cc), 0);
            gb_border(main, garea_with(gpoint_move(p, offset - 3, offset + 3), gsize_of(sq_size[s], sq_size[s])), sq_size[s] / 2, 2, 0x00cc0000);

            gb_blur(main, garea_with(p, tile_size), blur_radii[r], 0);
            gb_border(main, garea_with(p, tile_size), 0, 1, color_tango_dark_gray());
        }
    }

    graphics_display_main_buffer();
}

void shadows_demo() {
    gbuffer *main = graphics_get_main_buffer();
    // gb_fill(main, 0x008080); // Windows '95
    color tek_light = 0xFF0482AC;
    color nextstep_bg = 0xFF555577;
    color_params grad = color_params_gradient(nextstep_bg, color_darken(nextstep_bg, 0.12), gpoint_zero(), garea_bottom_right(main->area), ease_linear);
    gb_rect(main, main->area, grad, 0);

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
    //         gb_paint_pixel(main, i,     j, clr);
    //         gb_paint_pixel(main, i,     j + 1, clr);
    //         gb_paint_pixel(main, i + 1, j, clr);
    //         gb_paint_pixel(main, i + 1, j + 1, clr);
    //     }
    // }
/*
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
    gb_rect_border(rr, garea_of(0, 0, 220, 150), 16, 2, color_tango_black());
    
    gbuffer *rr_shadow = new_gbuffer(350, 350);
    gb_drop_shadow(rr_shadow, rr, shadow_params_of(0xff000000, 255, 1, 1, 6));
    
    gb_copy_area_with_alpha(main, rr_shadow, rr_shadow->area.size, gpoint_of(220, 100), gpoint_zero(), 0xFF);
    gb_copy_area_with_alpha(main, rr, rr->area.size, gpoint_of(220, 100), gpoint_zero(), 0xFF);

    graphics_display_main_buffer();
*/
}

void initialize_cpu() {
    log.info("Initializing IDT");
    initialize_idt();

    log.info("Remapping PIC IRQs");
    pic_remap_irqs();

    // disable everything, enable needed
    pic_disable_all_irqs();
    pic_enable_irq(0);   // timer
    pic_enable_irq(1);   // keyboard
    pic_enable_irq(2);   // cascade
    pic_enable_irq(12);  // mouse

    // final piece
    asm("sti");
}

void kernel_main(boot_info_t* bi) {
    // preserve boot info, asap
    asm("cli");
    memcpy(&global_boot_info, bi, sizeof(boot_info_t));
    initialize_graphics((char *)bi->fb.fb_addr, bi->fb.width, bi->fb.height, bi->fb.pitch, bi->fb.bpp);
    initialize_logger(LOG_LEVEL_DEBUG);


    log.info("Kernel starting...");
    // rectangles_borders_demo();
    // blend_demo();
    // fonts_demo();
    // gradient_demo();
    // blur_demo();
    // shadows_demo();

    initialize_cpu();
    initialize_mouse();

    
    // this might be the idle task, good enough for now
    for (;;) {
        // log.debug("looping...");
        keyboard_process(); // read scancodes, generate events
        mouse_process(); // read packets, generate events

        // wait for event:
        if (event_queue_empty(&global_event_queue))
            continue;

        event_t ev;
        event_queue_pop(&global_event_queue, &ev);
        log_event_as_info("kernel loop", &ev);
    }

    // kernel can never return, there's nothing to return to. it's all burned down to the ground.
    log.info("Kernel halted. Close the emulator.");
    for(;;) asm volatile("hlt");
}
