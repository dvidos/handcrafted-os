#include "framebuffer.h"
#include <stdint.h>

void kernel_main(framebuffer_info_t* fb) {
    
    uint32_t* pixels = (uint32_t*)fb->fb_addr;
    for (uint32_t i = 0; i < 100 && i < fb->width && i < fb->height; i++)
        pixels[i * (fb->pitch/4) + i] = 0x00FF00; // green

    for(;;) asm volatile("hlt");
}
