#include "multiboot2.h"
#include "vga.h"
#include "serial.h"
#include <stdint.h>

// --------------------------------------------------------------------------------------

typedef struct __attribute__((packed)) {
    uint32_t total_size;
    uint32_t reserved;
    // tags follow
} mb2_header_t;

typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t size;
    uint8_t data[]; // flexible array for tag-specific data
} mb2_tag_t;

void parse_multiboot2(uint32_t addr) {
    mb2_header_t* hdr = (mb2_header_t*)addr;
    
    vga_str("First 16 bytes of MB2 header: "); for (int i=0; i<16; i++) vga_hex8(((uint8_t*)addr)[i]); vga_str("\n");

    uint32_t total_size = hdr->total_size;
    vga_str("Multiboot2 total size: "); vga_int(total_size); vga_str("\n");
    uint8_t* ptr = (uint8_t*)addr + 8; // skip total_size + reserved
    while (ptr < (uint8_t*)addr + total_size) {
        mb2_tag_t* tag = (mb2_tag_t*)ptr;

        if (tag->type == 0) { // end tag
            break;
        }

        vga_str("Tag type: "); vga_int(tag->type); 
        vga_str(", size: "); vga_int(tag->size); 
        vga_str(", data: "); 

        // Example: print first 8 bytes of data in hex
        uint32_t data_bytes = tag->size - 8;
        for (uint32_t i = 0; i < data_bytes; i++) {
            vga_hex8(tag->data[i]);
            vga_str(" ");
        }
        vga_str("\n");

        // Tags are aligned to 8 bytes
        ptr += (tag->size + 7) & ~7;
    }
    vga_str("Done\n");
}

// ----------------------------------------------------------------------------------

void dump_mbi_bytes(multiboot_info_t* mbi) {
    uint8_t *ptr = (uint8_t *) mbi;
    for (int i = 0; i < 8; i++) {
        vga_hex8(ptr[i]);
        vga_char(' ');
    }
    vga_char('\n');
}



void kmain(void *magic_num, multiboot_info_t* mbi) {
    vga_str("Kernel started\n");
    vga_str("EAX magic num: "); vga_hex32((uint32_t)magic_num); vga_str("\n");
    vga_str("EBX multiboot info address: "); vga_hex32((uint32_t)mbi); vga_str("\n");

    parse_multiboot2((uint32_t)mbi);

    // Memory info
    vga_str("Memory info:\n");
    vga_str("Lower: "); vga_int(mbi->mem_lower); vga_char('\n');
    vga_str("Upper: "); vga_int(mbi->mem_upper); vga_char('\n');

    // Check if framebuffer is available
    if (mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO) {
        vga_str("Framebuffer info:\n");
        vga_str("Width: ");  vga_int(mbi->framebuffer_width);  vga_char('\n');
        vga_str("Height: "); vga_int(mbi->framebuffer_height); vga_char('\n');
        vga_str("BPP: ");    vga_int(mbi->framebuffer_bpp);    vga_char('\n');
        vga_str("Type: ");   vga_int(mbi->framebuffer_type);   vga_char('\n');
        vga_str("Pitch: ");  vga_int(mbi->framebuffer_pitch);  vga_char('\n');
    } else {
        vga_str("No framebuffer info provided\n");
    }

    // Draw a small green diagonal line if 32bpp RGB
    if ((mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO) &&
        mbi->framebuffer_bpp == 32 &&
        mbi->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
        
        uint8_t *fb = (uint8_t *)(uint32_t)mbi->framebuffer_addr;
        uint32_t width  = mbi->framebuffer_width;
        uint32_t height = mbi->framebuffer_height;
        uint32_t pitch  = mbi->framebuffer_pitch;
        uint8_t bpp     = mbi->framebuffer_bpp;

        for (uint32_t y = 0; y < width && y < height; y++) {
            uint32_t offset = y * pitch + y * (bpp / 8);
            *((uint32_t *)(fb + offset)) = 0x00FF00; // green pixel
        }

        vga_str("Drew diagonal line in framebuffer\n");
    }

    vga_str("Kernel halt\n");
    while(1);
}
