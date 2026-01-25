#pragma once
#include <stdint.h> // For uint8_t
#include <stddef.h> // For size_t

long parse_size(const char *size_str);
void hexdump_with_folding(const uint8_t *data, size_t size, size_t offset);
int error(const char *fmt, ...);
