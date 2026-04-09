#include "../libc_internal.h"


typedef struct input {
    const char *buffer;
    int size;
    int pos;
} input;

typedef struct output {
    char *buffer;
    int capacity;
    int pos;
    int length;
} output;

static inline char in_chr(input *ip) {
    if (ip->pos >= ip->size)
        return 0;

    char c = ip->buffer[ip->pos];
    ip->pos++;
    return c;
}

static inline void out_chr(output *op, char c) {
    if (op->pos < op->capacity - 1) {
        op->buffer[op->pos] = c;
        op->pos++;
        op->buffer[op->pos] = 0;
    }
    op->length++;
}

static void out_aligned(output *p, char *content, bool align_left, char padding, int width) {
    width -= strlen(content);

    if (!align_left) {
        while (width-- > 0)
            out_chr(p, padding);
    }

    while (*content != 0)
        out_chr(p, *content++);

    if (align_left) {
        while (width-- > 0)
            out_chr(p, padding);
    }
}

static void handle_selector(input *ip, output *op, va_list *args) {
    bool align_left = false;
    bool is_short = false;
    bool is_long = false;
    bool is_long_long = false;
    bool is_size = false;
    char padding = ' ';
    int width = 0;
    char tmp[64+1];
    char c;
    char *str;

    c = in_chr(ip);
    if (c == 0) return;

    if (c == '-') {
        align_left = true;
        c = in_chr(ip);
        if (c == 0) return;
    }
    if (c == '0') {
        padding = '0';
        c = in_chr(ip);
        if (c == 0) return;
    }
    while (c >= '0' && c <= '9') {
        width = width * 10 + (c - '0');
        c = in_chr(ip);
        if (c == 0) return;
    }
    if (c == 'h') {
        is_short = true;
        c = in_chr(ip);
        if (c == 0) return;

    } else if (c == 'l') {
        is_long = true;
        c = in_chr(ip);
        if (c == 0) return;

        if (c == 'l') {
            is_long_long = true;
            is_long = false;
            c = in_chr(ip);
            if (c == 0) return;
        }

    } else if (c == 'z') {
        is_size = true;
        c = in_chr(ip);
        if (c == 0) return;
    }

    switch (c) {
        case 'c':
            c = (char)va_arg(*args, int); // note arg promoted to int
            tmp[0] = c;
            tmp[1] = 0;
            out_aligned(op, tmp, align_left, padding, width);
            break;

        case 's':
            str = va_arg(*args, char *);
            if (str == NULL) str = "(null)";
            out_aligned(op, str, align_left, padding, width);
            break;

        case 'd': // signed decimal
        case 'i': // fallthrough
            if (is_short) {
                short s = (short)va_arg(*args, int); // note arg promoted to int
                ltoa((int)s, tmp, 10);
            } else if (is_long) {
                long l = (long)va_arg(*args, long);
                ltoa(l, tmp, 10);
            } else if (is_long_long) {
                long long ll = (long long)va_arg(*args, long long);
                lltoa(ll, tmp, 10);
            } else if (is_size) {
                ssize_t s = (ssize_t)va_arg(*args, ssize_t);
                ltoa((long)s, tmp, 10);
            } else {
                int i = (int)va_arg(*args, int);
                ltoa(i, tmp, 10);
            }
            out_aligned(op, tmp, align_left, padding, width);
            break;

        case 'u': // unsigned decimal
        case 'x': // hex, fallthrough
            int radix = (c == 'x') ? 16 : 10;

            if (is_short) {
                unsigned short s = (short)va_arg(*args, unsigned int); // note arg promoted to int
                ultoa((int)s, tmp, radix);
            } else if (is_long) {
                unsigned long l = (long)va_arg(*args, unsigned long);
                ultoa(l, tmp, radix);
            } else if (is_long_long) {
                unsigned long long ll = (long long)va_arg(*args, unsigned long long);
                ulltoa(ll, tmp, radix);
            } else if (is_size) {
                size_t s = (size_t)va_arg(*args, size_t);
                ultoa((long)s, tmp, radix);
            } else {
                unsigned int i = (unsigned int)va_arg(*args, unsigned int);
                ultoa(i, tmp, radix);
            }
            out_aligned(op, tmp, align_left, padding, width);
            break;

        case 'p': // pointer
            uintptr_t ptr = (uintptr_t)va_arg(*args, void *);
            tmp[0] = '0';
            tmp[1] = 'x';
            ultoa(ptr, tmp + 2, 16);
            out_aligned(op, tmp, align_left, padding, width);
            break;

        default:
            // not supported (e.g. %!), output as is
            tmp[0] = c;
            tmp[1] = 0;
            out_aligned(op, tmp, align_left, padding, width);
            break;
    }
}


/**
 * @brief Prints formatted output to a string with a specified size limit, using a `va_list` argument.
 *
 * This function is equivalent to `snprintf` but accepts a `va_list` object
 * that contains the variable arguments instead of taking them directly.
 *
 * @param str The character array to write the formatted output to.
 * @param size The maximum number of characters to write, including the null terminator.
 * @param format The format string.
 * @param ap A `va_list` object initialized by `va_start`.
 * @return On success, the total number of characters that would have been written
 *         if `size` had been sufficiently large (excluding the null terminator)
 *         is returned. On error, a negative value is returned.
 */
int vsnprintf(char *buffer, size_t buffer_size, const char *format, va_list args) {

    input ip = { .buffer = format, .size = strlen(format) };
    output op = { .buffer = buffer, .capacity = buffer_size };

    while (true) {
        char c = in_chr(&ip);
        if (c == 0) break;

        if (c == '%') {
            handle_selector(&ip, &op, &args);
        } else {
            out_chr(&op, c);
        }
    }

    return op.length;
}
