#include <stdarg.h>
#include "string.h"
#include "sprintf.h"

static void reverse(char *buffer, int len) {
    // reverse the buffer in place
    int left = 0;
    int right = len - 1;
    char c;
    while (left < right) {
        c = buffer[left];
        buffer[left] = buffer[right];
        buffer[right] = c;
        left++;
        right--;
    }

}

static void ltoa(long num, char *buffer, int base) {

    if (num == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    int negative = 0;
    if (base == 10 && num < 0) {
        negative = 1;
        num = num * -1;
    }

    int pos = 0;
    while (num > 0) {
        int remainder = num % base;
        buffer[pos++] = (remainder >= 10) ? ('A' + (remainder - 10)) : ('0' + remainder);
        num = num / base;
    }
    if (negative) {
        buffer[pos++] = '-';
    }
    buffer[pos] = '\0';
    reverse(buffer, pos);
}

static void ultoa(unsigned long num, char *buffer, int base) {

    if (num == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    int pos = 0;
    while (num > 0) {
        int remainder = num % base;
        buffer[pos++] = (remainder >= 10) ? ('A' + (remainder - 10)) : ('0' + remainder);
        num = num / base;
    }
    buffer[pos] = '\0';
    reverse(buffer, pos);
}

static void align(char **buffer, int *buffsize, char *content, int width, char padder, int pad_right) {
    width -= strlen(content);

    if (!pad_right) {
        while (width-- > 0 && *buffsize > 1) {
            **buffer = padder;
            (*buffer)++;
            (*buffsize)--;
        }
    }

    while (*content != '\0' && *buffsize > 1) {
        **buffer = *content++;
        (*buffer)++;
        (*buffsize)--;
    }

    if (pad_right) {
        while (width-- > 0 && *buffsize > 1) {
            **buffer = padder;
            (*buffer)++;
            (*buffsize)--;
        }
    }
}

// makes sure to include the zero terminator
// supports:
//   s: string
//   c: char
//   d, i: signed int in decimal
//   u: unsigned int in decimal
//   x: unsigned int in hex
//   o: unsigned int in octal
//   b: unsigned int in binary
//   p: pointer (unsigned int)
//   -: align left / pad right
//   0: pad with zeros, instead of spaces
//   <num>: width of padding
// maybe implemented in the huture:
//   h: short int flag
//   l: long int flag
//   f: float
//   e: scientific
//   .<num>: floating point precision
void vsprintfn(char *buffer, int buffsize, const char *format, va_list args) {

    int pad_right = 0;
    char padder = ' ';
    int width = 0;
    long l;
    unsigned long ul;
    char *ptr;
    char content[65]; // we may print at least 32 bits numbers in binary

    while (*format && buffsize > 1) {
        if (*format != '%') {
            *buffer++ = *format;
            if (--buffsize == 0)
                return;
            format++;
            continue;
        }

        // so definitely a '%'. let's see the next
        format++;
        padder = ' ';
        width = 0;
        pad_right = 0;
        if (*format == '-') {
            pad_right = 1;
            format++;
        }
        if (*format == '0') {
            padder = '0';
            format++;
        }
        while (*format >= '0' && *format <= '9') {
            width = width * 10 + (*format - '0');
            format++;
        }

        switch (*format) {
            case 'c':
                content[0] = (char)va_arg(args, int);
                content[1] = '\0';
                align(&buffer, &buffsize, content, width, padder, pad_right);
                break;
            case 's':
                ptr = va_arg(args, char *);
                if (ptr == 0)
                    ptr = "(null)";
                align(&buffer, &buffsize, ptr, width, padder, pad_right);
                break;
            case 'd': // signed decimal
            case 'i': // fallthrough
                l = (long)va_arg(args, int);
                ltoa(l, content, 10);
                align(&buffer, &buffsize, content, width, padder, pad_right);
                break;
            case 'u': // unsigned decimal
                ul = (unsigned int)va_arg(args, unsigned int);
                ultoa(ul, content, 10);
                align(&buffer, &buffsize, content, width, padder, pad_right);
                break;
            case 'x': // hex
                ul = (unsigned long)va_arg(args, unsigned int);
                ultoa(ul, content, 16);
                align(&buffer, &buffsize, content, width, padder, pad_right);
                break;
            case 'o': // octal
                ul = (unsigned long)va_arg(args, unsigned int);
                ultoa(ul, content, 8);
                align(&buffer, &buffsize, content, width, padder, pad_right);
                break;
            case 'b': // binary
                ul = (unsigned long)va_arg(args, unsigned int);
                ultoa(ul, content, 2);
                align(&buffer, &buffsize, content, width, padder, pad_right);
                break;
            case 'p': // pointer
                ul = (unsigned long)va_arg(args, void *);
                ultoa(ul, content, 16);
                align(&buffer, &buffsize, content, width, padder, pad_right);
                break;
            case '%':
                *buffer++ = *format;
                if (--buffsize == 0)
                    return;
                break;
            default:
                *buffer++ = '%';
                if (--buffsize == 0)
                    return;
                *buffer++ = *format;
                if (--buffsize == 0)
                    return;
                break;
        }
        if (buffsize == 0)
            return;

        format++;
    }
    *buffer = '\0';
}

void sprintfn(char *buffer, int buffsize, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsprintfn(buffer, buffsize, format, args);
    va_end(args);
}

