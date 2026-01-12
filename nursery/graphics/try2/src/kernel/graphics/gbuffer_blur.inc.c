#include "../fundamentals.h"
#include "gbuffer.h"
#include "../devices/serial.h"


typedef struct blur_window {
    uint32_t alpha;
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    int size;
} blur_window;


static inline void blur_window_clear(blur_window *w) {
    w->alpha = 0;
    w->red   = 0;
    w->green = 0;
    w->blue  = 0;
    w->size  = 0;
}

static inline void blur_window_add(blur_window *w, uint32_t *pixel) {
    w->alpha += color_a(*pixel);
    w->red   += color_r(*pixel);
    w->green += color_g(*pixel);
    w->blue  += color_b(*pixel);
    w->size  += 1;
}

static inline void blur_window_remove(blur_window *w, uint32_t *pixel) {
    w->alpha -= color_a(*pixel);
    w->red   -= color_r(*pixel);
    w->green -= color_g(*pixel);
    w->blue  -= color_b(*pixel);
    w->size  -= 1;
}

static inline void blur_window_add_and_remove(blur_window *w, uint32_t *add, uint32_t *remove) {
    w->alpha += color_a(*add) - color_a(*remove);
    w->red   += color_r(*add) - color_r(*remove);
    w->green += color_g(*add) - color_g(*remove);
    w->blue  += color_b(*add) - color_b(*remove);
}

typedef void blur_window_apply_func(blur_window *w, uint32_t *pixel);

static void blur_window_apply_color(blur_window *w, uint32_t *pixel) {
    if (w->size == 0) { _replace_pixel(pixel, 0xFFFF0000); return; }
    _replace_pixel(pixel, color_argb(color_a(_get_pixel(pixel)), w->red / w->size, w->green / w->size, w->blue / w->size));
}

static void blur_window_apply_alpha(blur_window *w, uint32_t *pixel) {
    if (w->size == 0) { _replace_pixel(pixel, 0xFFFF0000); return; }
    _replace_pixel(pixel, color_with_alpha(w->alpha / w->size, _get_pixel(pixel)));
}


#define DEBUG_BLUR(src, dest, slice_num, pixel_num, slice_start_pixel, slice_end_pixel, radius, win, extra)  ((void)0)
// #define DEBUG_BLUR(src, dest, slice_num, pixel_num, slice_start_pixel, slice_end_pixel, radius, win, extra)  __debug_blur(src, dest, slice_num, pixel_num, slice_start_pixel, slice_end_pixel, radius, win, extra)

static inline void __debug_blur(gbuffer *src, gbuffer *dest, int slice_num, int pixel_num, int slice_start_pixel, int slice_end_pixel, int radius, blur_window *win, char *extra) {
    serial_print_str("blur");
    serial_print_str(", radius="); serial_print_int(radius);
    serial_print_str(", slice="); serial_print_int(slice_num);
    serial_print_str(", range=["); serial_print_int(slice_start_pixel);serial_print_str(".."); serial_print_int(slice_end_pixel); serial_print_char(']');
    serial_print_str(", pixel="); serial_print_int(pixel_num);
    serial_print_str(", src_clr="); serial_print_hex32(_get_pixel(_pixel_ptr(src, pixel_num, slice_num)));
    serial_print_str(", w_size="); serial_print_int(win->size);
    if (win->size > 0) {
        serial_print_str(", w_avg="); 
        serial_print_hex32(color_argb(
            win->alpha / win->size,
            win->red   / win->size,
            win->green / win->size,
            win->blue  / win->size
        ));
    }
    serial_print_str(", dest_clr="); serial_print_hex32(_get_pixel(_pixel_ptr(dest, pixel_num, slice_num)));
    if (extra) {
        serial_print_str(", ");
        serial_print_str(extra);
    }
    serial_print_str("\r\n");
}

typedef uint32_t *blur_get_pixel_func(gbuffer *gb, int slice_num, int slice_px);

static uint32_t *blur_get_pixel_horizontal_slices(gbuffer *gb, int slice_num, int slice_px) {
    return _pixel_ptr(gb, slice_px, slice_num); // pixels are x, slices are y
}
static uint32_t *blur_get_pixel_vertical_slices(gbuffer *gb, int slice_num, int slice_px) {
    return _pixel_ptr(gb, slice_num, slice_px); // slices are x, pixels are y
}

static inline void blur_window_box_algorithm(gbuffer *src, gbuffer *dest,
    int start_slice, int end_slice, int slice_start_pixel, int slice_end_pixel,
    int radius, blur_get_pixel_func get_slice_pixel, blur_window_apply_func apply_blur) {
    // this method mirrors the same for vertical
    blur_window win;
    
    for (int slice_num = start_slice; slice_num < end_slice; slice_num++) {
        int pixel_num;
        int win_start_pixel;
        int win_end_pixel;
        
        blur_window_clear(&win);
        DEBUG_BLUR(src, dest, slice_num, pixel_num, slice_start_pixel, slice_end_pixel, radius, &win, "init");

        // Prime window with pixels prior to the first pixel (partial window)
        for (int look_back = radius; look_back > 0; look_back--) {
            int px = slice_start_pixel - look_back;
            if (px < 0)
                continue;

            blur_window_add(&win, get_slice_pixel(src, slice_num, px));
            DEBUG_BLUR(src, dest, slice_num, pixel_num, slice_start_pixel, slice_end_pixel, radius, &win, "priming");
        }

        // Phase 1: growing window (left edge),  x = [x_start .. x_start+radius-1]
        for (pixel_num = slice_start_pixel; pixel_num <= slice_start_pixel + radius && pixel_num < slice_end_pixel; pixel_num++) {
            win_end_pixel = pixel_num + radius;

            if (win_end_pixel < slice_end_pixel)
                blur_window_add(&win, get_slice_pixel(src, slice_num, win_end_pixel));
            apply_blur(&win, get_slice_pixel(dest, slice_num, pixel_num));
            DEBUG_BLUR(src, dest, slice_num, pixel_num, slice_start_pixel, slice_end_pixel, radius, &win, "growing");
        }

        // Phase 2: full window, fast path, x = [x_start+radius .. x_end-radius-1]
        for (; pixel_num + radius < slice_end_pixel; pixel_num++) {
            win_start_pixel = pixel_num - radius - 1;  // add right, remove left
            win_end_pixel = pixel_num + radius;

            blur_window_add_and_remove(&win, get_slice_pixel(src, slice_num, win_end_pixel), get_slice_pixel(src, slice_num, win_start_pixel));
            apply_blur(&win, get_slice_pixel(dest, slice_num, pixel_num));
            DEBUG_BLUR(src, dest, slice_num, pixel_num, slice_start_pixel, slice_end_pixel, radius, &win, "stepping");
        }

        // Phase 3: shrinking window (right edge), x = [x_end-radius .. x_end-1]
        for (; pixel_num < slice_end_pixel; pixel_num++) {
            win_start_pixel = pixel_num - radius - 1;
            win_end_pixel = pixel_num + radius;

            if (win_start_pixel >= 0)
                blur_window_remove(&win, get_slice_pixel(src, slice_num, win_start_pixel));
            if (win_end_pixel < src->area.width)
                blur_window_add(&win, get_slice_pixel(src, slice_num, win_end_pixel));
            apply_blur(&win, get_slice_pixel(dest, slice_num, pixel_num));
            DEBUG_BLUR(src, dest, slice_num, pixel_num, slice_start_pixel, slice_end_pixel, radius, &win, "shrinking");
        }
    }
}

void gb_blur(gbuffer *gb, area rect, int radius, int blur_alpha_instead_of_color) {
    rect = area_crop(rect, gb->area);
    if (area_is_empty(rect))
        return;
    if (radius <= 0)
        return;
    
    /*
        update: i used gaussian blur, which is ideal mathematically, but painfully slow.
        i even had to create external tool to generate precal tables for radii 1..32.
        it seems box blur (x3) achieves similar results. box blur is essentially adding average of sourounding pixels.
        one pass horizontal, one vertical, then again, two more times (or more).
        so, box blur is a building block, not a final product.
        if one uses a running window, speed can be improved dramatically.
        the window only works in the "middle", where the size of the window can be full.
        i should make a building block (actually two, for x/y directions) in the include file,
        then call it 6 times here.
    */

    // copy some zone from the original image, into the aux buffer, 
    // so that vertical passes do not bleed into unknown pixels.
    gb_copy_area_fast(global_aux_buffer, gb, size_of(rect.width, radius), point_of(rect.x, rect.y - radius),           point_of(rect.x, rect.y - radius));
    gb_copy_area_fast(global_aux_buffer, gb, size_of(rect.width, radius), point_of(rect.x, rect.y + rect.height), point_of(rect.x, rect.y + rect.height));

    blur_window_apply_func *color_applicator = (blur_alpha_instead_of_color ? blur_window_apply_alpha : blur_window_apply_color);

    for (int times = 0; times < 3; times++) {
        blur_window_box_algorithm(
            gb, global_aux_buffer, 
            rect.y, rect.y + rect.height,
            rect.x, rect.x + rect.width,
            radius, blur_get_pixel_horizontal_slices, color_applicator
        );
        blur_window_box_algorithm(
            global_aux_buffer, gb,
            rect.x, rect.x + rect.width,
            rect.y, rect.y + rect.height,
            radius, blur_get_pixel_vertical_slices, color_applicator
        );
    }
}


