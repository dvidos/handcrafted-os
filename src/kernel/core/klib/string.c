#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <drivers/screen.h>


int strlen(const char* str)
{
	int len = 0;
	while (str[len])
		len++;
	return len;
}

int strcmp(char *a, char *b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return (int)(*a - *b);
        }
        a++;
        b++;
    }
    // either one or both are '\0'
    return (int)(*a - *b);
}

void strcpy(char *target, char *source) {
    while (*source != '\0') {
        *target++ = *source++;
    }
    *target = *source; // final null char
}

void strncpy(char *target, char *source, size_t size) {
    while (*source != '\0' && size-- > 1) {
        *target++ = *source++;
    }

    // in any case, we either have the source pointing to the zero terminator
    // or we only have space left for one character in target.
    *target = '\0';
}

void memset(void *dest, char value, size_t size) {
    char *d = (char *)dest;
    while (size-- > 0) {
        *d++ = value;
    }
}


void memcpy(void *dest, void *source, size_t size) {
    char *d = (char *)dest;
    char *s = (char *)source;
    while (size-- > 0) {
        *d++ = *s++;
    }
}

int memcmp(void *a, void *b, size_t size) {
    char *ca = (char *)a;
    char *cb = (char *)b;
    while (size-- > 0) {
        if ((*ca) != (*cb))
            return (int)((*ca) - (*cb));
        ca++;
        cb++;
    }
    return 0;
}

char *memmove(void *dest, void *source, size_t size) {
    char *d = dest;
    char *s = source;

    if (dest > source && dest < (source + size)) {
        // if destination resides inside source, 
        // we must go from end to start
        d += size - 1;
        s += size - 1;
        while (size-- > 0) {
            *d-- = *s--;
        }
    } else {
        while (size-- > 0) {
            *d++ = *s++;
        }
    }

    return dest;
}


void reverse(char *buffer, int len) {
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

void ltoa(long num, char *buffer, int base) {

    if (num == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    bool negative = false;
    if (base == 10 && num < 0) {
        negative = true;
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

void ultoa(unsigned long num, char *buffer, int base) {

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

char *strchr(char *str, char c) {
    while (*str != '\0') {
        if (*str == c)
            return str;
        str++;
    }
    return NULL;
}

char *strstr(char *haystack, char *needle) {
    // there's ton of room for improvement and complex algorithms here
    // but for now, we just want an implementation that works, even a slow one

    if (needle == NULL || needle[0] == '\0')
        return haystack;

    char *start = haystack;
    int needle_len = strlen(needle);
    while (*start != '\0') {
        char *pos = strchr(start, needle[0]);
        if (pos == NULL)
            return NULL; // first char not found
        if (strlen(pos) < needle_len)
            return NULL; // not enough haystack to check the needle
        if (memcmp(pos, needle, needle_len) == 0)
            return pos; // the whole needle matched (no terminators)
        start = pos + 1; // skip this match, search next from subsquent byte
    }

    return NULL;
}


// first call str has to have a string, subsequent calls must pass NULL
char *strtok(char *str, char *delimiters) {
    // printf("strtok(\"%s\", \"%s\")\n", str, delimiters);
    static char *next = NULL;

    // if NULL is passed, remember last tokenized pointer
    if (str == NULL) {
        if (next == NULL) // user has not called us with valid string yet
            return NULL;
        str = next;
        // printf("strtok(): Continuing at... [%s]\n", str);
    }

    // skip over delimiters in the start of the string
    // printf("strtok(): Skipping over delimiters [%s]\n", delimiters);
    while (*str != '\0' && strchr(delimiters, *str) != NULL) {
        // not sure this is compliant, but we null out all delimiters
        *str = '\0'; 
        str++;

    }
    if (*str == '\0') {
        return NULL; // we reached the end
    }

    // find the next delimiter, we are on a non-delimiter character
    // printf("strtok(): Start of token is [%c]\n", *str);
    char *end = str;
    while (true) {
        if (*end == '\0') {
            next = NULL;
            return str;
        } else if (strchr(delimiters, *end) != NULL) {
            // we found a delimiter
            // printf("strtok(): Found delimiter [%c]\n", *end);
            *end = '\0';
            next = end + 1; // save for later
            return str;
        }
        end++;
    }
}

int atoi(char *str) {
    while (*str == ' ')
        str++;
    if (*str == '\0')
        return 0;

    bool negative = false;
    if (*str == '-') {
        negative = true;
        str++;
    }

    int radix;
    if (*str == 'x') {
        radix = 16;
        str++;
    } else if (*str == '0' && *(str+1) == 'x') {
        radix = 16;
        str += 2;
    } else if (*str == '0') {
        radix = 8;
        str++;
    } else if (*str == 'b') {
        radix = 2;
        str++;
    } else {
        radix = 10;
    }

    int value = 0;
    while (*str != '\0') {
        int digit_value = 0;
        if (*str >= 'A' && *str <= 'F' && radix == 16) {
            digit_value = 10 + ((*str) - 'A');
        } else if (*str >= 'a' && *str <= 'f' && radix == 16) {
            digit_value = 10 + ((*str) - 'a');
        } else if (*str >= '0' && *str <= '9' && (radix == 10 || radix == 16)) {
            digit_value = (*str) - '0';
        } else if (*str >= '0' && *str <= '7' && radix == 8) {
            digit_value = (*str) - '0';
        } else if (*str >= '0' && *str <= '1' && radix == 2) {
            digit_value = (*str) - '0';
        } else {
            break;
        }
        value = value * radix + digit_value;
        str++;
    }

    if (negative)
        value = value * (-1);

    return value;
}

unsigned int atoui(char *str) {
    while (*str == ' ')
        str++;
    if (*str == '\0')
        return 0;

    int radix;
    if (*str == 'x') {
        radix = 16;
        str++;
    } else if (*str == '0' && *(str+1) == 'x') {
        radix = 16;
        str += 2;
    } else if (*str == '0') {
        radix = 8;
        str++;
    } else if (*str == 'b') {
        radix = 2;
        str++;
    } else {
        radix = 10;
    }

    unsigned int value = 0;
    while (*str != '\0') {
        unsigned int digit_value = 0;
        if (*str >= 'A' && *str <= 'F' && radix == 16) {
            digit_value = 10 + ((*str) - 'A');
        } else if (*str >= 'a' && *str <= 'f' && radix == 16) {
            digit_value = 10 + ((*str) - 'a');
        } else if (*str >= '0' && *str <= '9' && (radix == 10 || radix == 16)) {
            digit_value = (*str) - '0';
        } else if (*str >= '0' && *str <= '7' && radix == 8) {
            digit_value = (*str) - '0';
        } else if (*str >= '0' && *str <= '1' && radix == 2) {
            digit_value = (*str) - '0';
        } else {
            break;
        }
        value = value * radix + digit_value;
        str++;
    }

    return value;
}


static void sprintn_aligned(char **buffer, int *buffsize, char *content, int width, char padder, bool pad_right) {
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

    bool pad_right = false;
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
        pad_right = false;
        if (*format == '-') {
            pad_right = true;
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
                sprintn_aligned(&buffer, &buffsize, content, width, padder, pad_right);
                break;
            case 's':
                ptr = va_arg(args, char *);
                if (ptr == NULL)
                    ptr = "(null)";
                sprintn_aligned(&buffer, &buffsize, ptr, width, padder, pad_right);
                break;
            case 'd': // signed decimal
            case 'i': // fallthrough
                l = (long)va_arg(args, int);
                ltoa(l, content, 10);
                sprintn_aligned(&buffer, &buffsize, content, width, padder, pad_right);
                break;
            case 'u': // unsigned decimal
                ul = (unsigned int)va_arg(args, unsigned int);
                ultoa(ul, content, 10);
                sprintn_aligned(&buffer, &buffsize, content, width, padder, pad_right);
                break;
            case 'x': // hex
                ul = (unsigned long)va_arg(args, unsigned int);
                ultoa(ul, content, 16);
                sprintn_aligned(&buffer, &buffsize, content, width, padder, pad_right);
                break;
            case 'o': // octal
                ul = (unsigned long)va_arg(args, unsigned int);
                ultoa(ul, content, 8);
                sprintn_aligned(&buffer, &buffsize, content, width, padder, pad_right);
                break;
            case 'b': // binary
                ul = (unsigned long)va_arg(args, unsigned int);
                ultoa(ul, content, 2);
                sprintn_aligned(&buffer, &buffsize, content, width, padder, pad_right);
                break;
            case 'p': // pointer
                ul = (unsigned long)va_arg(args, void *);
                ultoa(ul, content, 16);
                sprintn_aligned(&buffer, &buffsize, content, width, padder, pad_right);
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


int tolower(int c) {
    return (c >= 'A' && c <= 'Z') ? (c | 0x20) : c;
}

int toupper(int c) {
    return (c >= 'a' && c <= 'z') ? (c & ~(int)0x20) : c;
}

// convert a UCS-2 string to an old-style C string, as possible
void ucs2str_to_cstr(char *ucs2str, char *cstr) {
    uint16_t ucs2_char = *(uint16_t *)ucs2str;
    while (ucs2_char != 0x0000) {
        if (ucs2_char >= 0x20 && ucs2_char <= 0x7e) {
            // simple ascii char
            *cstr++ = (char)ucs2_char;
        } else {
            // special character
            *cstr++ = '?';
        }
        ucs2str += 2;
        ucs2_char = *(uint16_t *)ucs2str;
    }
    *cstr = '\0';
}


