#include "fundamentals.h"
#include "strings.h"

#ifdef STANDALONE

size_t strlen(const char* str) {
	size_t len = 0;
	while (str[len])
		len++;
	return len;
}

void strcpy(char *target, const char *source) {
    while (*source != '\0') {
        *target++ = *source++;
    }
    *target = *source; // final null char
}

void strcat(char *target, const char *src) {
    
    char *dest = target;
    while (*dest != '\0')
        dest++;
    
    while (*src != '\0') {
        *dest++ = *src++;
    }
    *dest = *src; // final null char
}

int strcmp(const char* a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b)
            return (*a - *b);
        a++;
        b++;
    }
    if (*a == '\0' && *b != '\0')
        return 1;
    if (*b == '\0' && *a != '\0')
        return -1;
    return 0;
}

void memcpy(void* destination, const void* source, size_t size) {
	unsigned char* dst = (unsigned char*) destination;
	const unsigned char* src = (const unsigned char*) source;
	for (size_t i = 0; i < size; i++)
		dst[i] = src[i];
}

int memcmp(const void* aptr, const void* bptr, size_t size) {
	const unsigned char* a = (const unsigned char*) aptr;
	const unsigned char* b = (const unsigned char*) bptr;
	for (size_t i = 0; i < size; i++) {
		if (a[i] < b[i])
			return -1;
		else if (b[i] < a[i])
			return 1;
	}
	return 0;
}

void memset(void* bufptr, char value, size_t size) {
	unsigned char* buf = (unsigned char*) bufptr;
	for (size_t i = 0; i < size; i++)
		buf[i] = (unsigned char) value;
}

// -----------------------------------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------------------------------

static void snprintf_aligned(char *buffer, int buffer_size, int *write_pos, char *content, int width, char padder, bool pad_right) {
    int pad_length = width - strlen(content);

    if (!pad_right) {
        while (pad_length-- > 0) {
            if (*write_pos < buffer_size) buffer[*write_pos] = padder;
            (*write_pos)++;
        }
    }
    while (*content != '\0') {
        if (*write_pos < buffer_size) buffer[*write_pos] = *content++;
        (*write_pos)++;
    }
    if (pad_right) {
        while (pad_length-- > 0) {
            if (*write_pos < buffer_size) buffer[*write_pos] = padder;
            (*write_pos)++;
        }
    }
}

// int vsprintfn(char *buffer, int buffsize, const char *format, va_list args)
// ---------------------------------------------------------------------------
// always zero terminates the buffer. does not overflow.
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
// returns the formatted length, even if larger than the buffer.
int vsprintfn(char *buffer, int buffsize, const char *format, va_list args) {
    bool pad_right = false;
    char c, padder = ' ';
    int write_pos = 0, width = 0;
    long l;
    unsigned long ul;
    char *ptr;
    char content[65]; // we may print at least 32 bits numbers in binary


    while (*format) {
        c = *format;

        // if not escape char, present it
        if (c != '%') {
            if (write_pos < buffsize) buffer[write_pos] = c;
            write_pos++;
            format++;
            continue;
        }

        // so definitely an escape. check flags
        c = *(++format);
        padder = ' ';
        width = 0;
        pad_right = false;
        if (c == '-') {
            pad_right = true;
            c = *(++format);
        }
        if (c == '0') {
            padder = '0';
            c = *(++format);
        }
        while (c >= '0' && c <= '9') {
            width = width * 10 + (c - '0');
            c = *(++format);
        }

        switch (c) {
            case 'c':
                content[0] = (char)va_arg(args, int);
                content[1] = '\0';
                snprintf_aligned(buffer, buffsize, &write_pos, content, width, padder, pad_right);
                break;
            case 's':
                ptr = va_arg(args, char *);
                if (ptr == NULL)
                    ptr = "(null)";
                snprintf_aligned(buffer, buffsize, &write_pos, ptr, width, padder, pad_right);
                break;
            case 'd': // signed decimal
            case 'i': // fallthrough
                l = (long)va_arg(args, int);
                ltoa(l, content, 10);
                snprintf_aligned(buffer, buffsize, &write_pos, content, width, padder, pad_right);
                break;
            case 'u': // unsigned decimal
                ul = (unsigned int)va_arg(args, unsigned int);
                ultoa(ul, content, 10);
                snprintf_aligned(buffer, buffsize, &write_pos, content, width, padder, pad_right);
                break;
            case 'x': // hex
                ul = (unsigned long)va_arg(args, unsigned int);
                ultoa(ul, content, 16);
                snprintf_aligned(buffer, buffsize, &write_pos, content, width, padder, pad_right);
                break;
            case 'o': // octal
                ul = (unsigned long)va_arg(args, unsigned int);
                ultoa(ul, content, 8);
                snprintf_aligned(buffer, buffsize, &write_pos, content, width, padder, pad_right);
                break;
            case 'b': // binary
                ul = (unsigned long)va_arg(args, unsigned int);
                ultoa(ul, content, 2);
                snprintf_aligned(buffer, buffsize, &write_pos, content, width, padder, pad_right);
                break;
            case 'p': // pointer
                ul = (unsigned long)va_arg(args, void *);
                ultoa(ul, content, 16);
                snprintf_aligned(buffer, buffsize, &write_pos, content, width, padder, pad_right);
                break;
            case '%': // an escaped '%'
                if (write_pos < buffsize) buffer[write_pos] = c;
                write_pos++;
                break;
            default: // an unsupported escape char
                if (write_pos < buffsize) buffer[write_pos] = '%';
                write_pos++;
                if (write_pos < buffsize) buffer[write_pos] = c;
                write_pos++;
                break;
        }
        format++;
    }

    if (buffsize > 0) {
        // null terminate somewhere
        int terminator = write_pos < buffsize ? write_pos : buffsize - 1;
        buffer[terminator] = 0;
    }

    // return the expanded length, without null terminator
    return write_pos;
}

int sprintfn(char *buffer, int buffsize, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int len = vsprintfn(buffer, buffsize, format, args);
    va_end(args);

    return len;
}

#endif // STANDALONE