#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

typedef struct framebuffer_info {
    uint32_t fb_addr;  // physical address
    uint32_t width;
    uint32_t height;
    uint32_t pitch;    // bytes per row
    uint32_t bpp;      // bits per pixel
} framebuffer_info_t;

typedef struct boot_info {
    framebuffer_info_t fb;
} boot_info_t;

#endif
