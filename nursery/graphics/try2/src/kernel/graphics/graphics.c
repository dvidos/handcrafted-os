#include <stdint.h>
#include "graphics.h"
#include "color.h"
#include "fonts/font8x16.h"
#include "gbuffer.h"
#include "../memory/string.h"

struct graphics_global_info {
    uint8_t *fb_address;
    int      fb_width;
    int      fb_height;
    int      fb_pitch;
    int      fb_bpp;
    gbuffer *main_buffer;
    gbuffer *aux_buffer; // e.g. for blurring
};
struct graphics_global_info ggi;


void initialize_graphics(void *fb_address, int width, int height, int pitch, int bpp) {
    ggi.fb_address = fb_address;
    ggi.fb_width = width;
    ggi.fb_height = height;
    ggi.fb_pitch = pitch;
    ggi.fb_bpp = bpp;

    ggi.main_buffer = new_gbuffer(width, height);
    ggi.aux_buffer = new_gbuffer(width, height);
    initialize_gbuffer(ggi.aux_buffer);
    initialize_colors();
}

void graphics_fill(color clr) {
    // assumes 24 bpp framebuffer
    uint8_t red   = color_r(clr);
    uint8_t green = color_g(clr);
    uint8_t blue  = color_b(clr);

    for (int y = 0; y < ggi.fb_height; y++) {
        uint8_t *ptr = ((uint8_t *)ggi.fb_address) + y * ggi.fb_pitch;
        for (int x = 0; x < ggi.fb_width; x++) {
            *ptr++ = blue;
            *ptr++ = green;
            *ptr++ = red;
        }
    }
}

void graphics_rect(int x, int y, int width, int height, color clr) {
    // assumes 24 bpp framebuffer
    uint8_t red   = color_r(clr);
    uint8_t green = color_g(clr);
    uint8_t blue  = color_b(clr);

    if (x + width > ggi.fb_width)
        width = ggi.fb_width - x;
    if (y + height > ggi.fb_height)
        height = ggi.fb_height - y;
    
    for (int y_offs = 0; y_offs < height; y_offs++) {
        uint8_t *ptr = ((uint8_t *)ggi.fb_address) + (y + y_offs) * ggi.fb_pitch + (x * 3);
        for (int x = 0; x < width; x++) {
            *ptr++ = blue;
            *ptr++ = green;
            *ptr++ = red;
        }
    }
}

static void inline graphics_pixel(int x, int y, color clr) {
    // assumes 24 bpp framebuffer
    if (x >= ggi.fb_width) return;
    if (y >= ggi.fb_height) return;
    
    uint8_t *ptr = ggi.fb_address + y * ggi.fb_pitch + x * 3;
    *ptr++ = color_b(clr);
    *ptr++ = color_g(clr);
    *ptr++ = color_r(clr);
}

void graphics_demo(int left, int top, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            graphics_pixel(left + x, top + y, color_rgb(x, y, x^y));
        }
    }
}

int graphics_draw_8x16_character(int x, int baseline_y, char chr, font8x16 *font, color clr) {
    const glyph8x16 *gl = font8x16_get_glyph(font, chr);

    for (int row_no = 0; row_no < font->num_bitmaps; row_no++) {
        uint8_t bitmap = gl->bitmaps[row_no];
        if (bitmap == 0)
            continue;

        for (int column = 0; column < gl->width; column++) {
            if ((bitmap & (0x80 >> column)) == 0)
                continue;
          
            graphics_pixel(x + column, baseline_y - font->baseline + row_no, clr);
        }
    }

    return gl->width;
}

int graphics_draw_8x16_text(int x, int baseline_y, const char *text, font8x16 *font, color clr) {
    int running_x = x;
    int width = 0;

    while (*text) {
        width = graphics_draw_8x16_character(running_x, baseline_y, *text, font, clr);
        running_x += width + font->char_spacing;
        text++;
    }

    return running_x - x;
}

int graphics_draw_8x16_demo(int x, int baseline_y, font8x16 *font, color clr) {
    graphics_draw_8x16_text(x, baseline_y, font->name, font, clr);
    baseline_y += font->num_bitmaps + 1;
    graphics_draw_8x16_text(x, baseline_y, "ABCDEFGHIJKLMNOPQRSTUVWXYZ 1234567890 {[(<>)]} \\|/", font, clr);
    baseline_y += font->num_bitmaps + 1;
    graphics_draw_8x16_text(x, baseline_y, "abcdefghijklmnopqrstuvwxyz `~!@#$%^&*-_=+;':\",.?", font, clr);
    baseline_y += font->num_bitmaps + 1;
    graphics_draw_8x16_text(x, baseline_y, "The quick brown fox jumped over the lazy dog!", font, clr);
}

gbuffer *graphics_get_main_buffer() {
    // this allows clients to manipulate the main buffer
    return ggi.main_buffer;
}
gbuffer *graphics_get_aux_buffer() {
    // this allows clients to manipulate the main buffer
    return ggi.aux_buffer;
}

int graphics_display_main_buffer() {
    // we need to convert formats.
    // our buffers are AARRGGBB, our VBE framebuffer is RRGGBB.
    uint8_t *vbe = ggi.fb_address;
    uint32_t *buff_argb = ggi.main_buffer->buffer_argb;
    int count = (ggi.fb_height * ggi.fb_pitch);

    if (ggi.fb_bpp == 32) {
        memcpy(vbe, buff_argb, count);
    } else if (ggi.fb_bpp == 24) {
        while (count-- > 0) {
            // alpha channel is lost here.
            *vbe++ = color_b(*buff_argb);
            *vbe++ = color_g(*buff_argb);
            *vbe++ = color_r(*buff_argb);
            buff_argb++;
        }
    }
}
