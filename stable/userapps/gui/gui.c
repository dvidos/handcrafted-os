#include <hcos/syslog.h>
#include <hcos/misc.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>


void fatal(char *msg) {
    syslog_critical(msg);
    for(;;);
}

struct fbinfo {
    void *addr;
    int width;
    int height;
    int bpp;
    int pitch;
} fb;


int main(int argc, char *argv[]) {
    syslog_info("gui running...");

    // somehow we should get info of Framebuffer, maybe in an environment variable, then do something with it.
    // should be in the form "addr=x0123123 size=1234x456 bpp=32 pitch=4096"
    char *framebuffer = getenv("FRAMEBUFFER");
    syslog_info("framebuffer variable is \"%s\"", framebuffer);
    if (framebuffer == NULL || strlen(framebuffer) == 0)
        fatal("FRAMEBUFFER variable not set or empty");
    
    char *token;
    char *rest = framebuffer;

    while ((token = strtok_r(rest, " ", &rest))) {
        if (strncmp(token, "addr=0x", 7) == 0) {
            fb.addr = (void *)strtoul(token + 7, 16, NULL);
        } else if (strncmp(token, "size=", 5) == 0) {
            char *size_str = token + 5;
            char *x_pos = strchr(size_str, 'x');
            if (x_pos) {
                *x_pos = '\0';
                fb.width = atoi(size_str);
                fb.height = atoi(x_pos + 1);
            }
        } else if (strncmp(token, "bpp=", 4) == 0) {
            fb.bpp = atoi(token + 4);
        } else if (strncmp(token, "pitch=", 6) == 0) {
            fb.pitch = atoi(token + 6);
        }
    }

    printf("Parsed 0x%x %dx%d@%d %d\n", fb.addr, fb.width, fb.height, fb.bpp, fb.pitch);

    syslog_info("Framebuffer info: addr=%p, width=%d, height=%d, bpp=%d, pitch=%d",
                fb.addr, fb.width, fb.height, fb.bpp, fb.pitch);

    if (fb.addr == NULL || fb.width == 0 || fb.height == 0 || fb.bpp == 0 || fb.pitch == 0) {
        fatal("Failed to parse all FRAMEBUFFER information.");
    }
    // Allocate buffer for RGBA pixels (4 bytes per pixel)
    size_t buffer_size = fb.width * fb.height * 4;
    unsigned char *prepared_buffer = (unsigned char *)malloc(buffer_size);
    if (prepared_buffer == NULL) {
        fatal("Failed to allocate memory for prepared buffer");
    }

    // Fill buffer with gradient: red to green from left to right, blue to green from top to bottom
    for (int y = 0; y < fb.height; ++y) {
        for (int x = 0; x < fb.width; ++x) {
            float x_normalized = (float)x / (fb.width - 1);
            float y_normalized = (float)y / (fb.height - 1);

            // Red: decreases from left to right
            unsigned char r = (unsigned char)(255 * (1.0f - x_normalized));
            // Blue: decreases from top to bottom
            unsigned char b = (unsigned char)(255 * (1.0f - y_normalized));
            // Green: increases with both x and y, averaged
            unsigned char g = (unsigned char)(255 * (x_normalized + y_normalized) / 2.0f);

            unsigned char a = 255; // Alpha: fully opaque

            // Assuming RGBA format in the buffer (R, G, B, A)
            unsigned char *pixel = prepared_buffer + (y * fb.width + x) * 4;
            pixel[0] = r;
            pixel[1] = g;
            pixel[2] = b;
            pixel[3] = a;
        }
    }

    // Memcpy prepared buffer to framebuffer
    memcpy(fb.addr, prepared_buffer, buffer_size);

    // Free the allocated buffer
    free(prepared_buffer);

    return 0;
}
