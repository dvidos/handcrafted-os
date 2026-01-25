#include "utils.h"
#include <stdio.h>
#include <stdlib.h> // For atol
#include <string.h> // For strlen, memcmp
#include <ctype.h>  // For tolower
#include <stdbool.h> // For bool

long parse_size(const char *size_str) {
    if (!size_str) return 0;
    long size = atol(size_str);
    char multiplier = tolower(size_str[strlen(size_str) - 1]);
    switch (multiplier) {
        case 'k': size *= 1024; break;
        case 'm': size *= 1024 * 1024; break;
        case 'g': size *= 1024 * 1024 * 1024; break;
    }
    return size;
}

void hexdump_with_folding(const uint8_t *data, size_t size, size_t offset) {
    const int BYTES_PER_LINE = 16;
    uint8_t prev_line[BYTES_PER_LINE];
    bool in_folded_block = false;

    for (size_t i = 0; i < size; i += BYTES_PER_LINE) {
        const uint8_t *current_line = data + i;
        bool is_same_as_prev = (i > 0 && memcmp(current_line, prev_line, BYTES_PER_LINE) == 0);

        if (is_same_as_prev) {
            if (!in_folded_block) {
                printf("*\n");
                in_folded_block = true;
            }
        } else {
            in_folded_block = false;
            printf("%08zx: ", offset + i);
            for (int j = 0; j < BYTES_PER_LINE; j++) {
                if (i + j < size) {
                    printf("%02x ", current_line[j]);
                } else {
                    printf("   "); // Pad if line is not full
                }
            }
            printf(" |");
            for (int j = 0; j < BYTES_PER_LINE; j++) {
                if (i + j < size) {
                    printf("%c", isprint(current_line[j]) ? current_line[j] : '.');
                } else {
                    printf(" ");
                }
            }
            printf("|\n");
            memcpy(prev_line, current_line, BYTES_PER_LINE);
        }
    }
    // After loop, if still in folded block, print the actual last line
    if (in_folded_block) {
        size_t last_line_start_offset = (size / BYTES_PER_LINE) * BYTES_PER_LINE;
        const uint8_t *last_line = data + last_line_start_offset;
        
        printf("%08zx: ", offset + last_line_start_offset);
        for (int j = 0; j < BYTES_PER_LINE; j++) {
            if (last_line_start_offset + j < size) {
                printf("%02x ", last_line[j]);
            } else {
                printf("   ");
            }
        }
        printf(" |");
        for (int j = 0; j < BYTES_PER_LINE; j++) {
            if (last_line_start_offset + j < size) {
                printf("%c", isprint(last_line[j]) ? last_line[j] : '.');
            } else {
                printf(" ");
            }
        }
        printf("|\n");
    }
}

#include <stdarg.h> // For va_list, va_start, va_end

int error(const char *fmt, ...) {
    fprintf(stderr, "Error: ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n"); // Add a newline for consistency
    return 1;
}
