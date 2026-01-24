#include "fundamentals.h"
#include "log_appender.h"
#include "mem_allocator.h"
#include "strings.h"

#ifdef HOSTED
    #include <stdio.h>
#endif

typedef struct memory_log_appender {
    log_appender appender;
    mem_allocator *allocator;

    char *buffer;
    u32 capacity;
    u32 length;
} memory_log_appender;

static void _memory_appender_append(log_appender *a, const char *line) {
    memory_log_appender *p = container_of(a, memory_log_appender, appender);

    u32 line_len = strlen(line);
    if (p->length + line_len + 1 > p->capacity) {
        p->capacity *= 2;
        char *new_buffer = p->allocator->allocate(p->allocator, p->capacity);
        strcpy(new_buffer, p->buffer);
        p->allocator->release(p->allocator, p->buffer);
        p->buffer = new_buffer;
    }
    
    strcpy(p->buffer + p->length, line);
    p->length += line_len;
}

static const char *_memory_appender_peek_buffer(log_appender *a) {
    memory_log_appender *p = container_of(a, memory_log_appender, appender);
    return p->buffer;
}

static void _memory_appender_destroy(log_appender *a) {
    memory_log_appender *p = container_of(a, memory_log_appender, appender);
    p->allocator->release(p->allocator, p->buffer);
    p->allocator->release(p->allocator, p);
}

log_appender *create_in_memory_log_appender(mem_allocator *allocator) {
    memory_log_appender *p = allocator->allocate(allocator, sizeof(memory_log_appender));

    p->appender.append = _memory_appender_append;
    p->appender.peek_buffer = _memory_appender_peek_buffer;
    p->appender.destroy = _memory_appender_destroy;

    p->capacity = 256;
    p->buffer = allocator->allocate(allocator, p->capacity);
    p->length = 0;

    return &p->appender;
}

// ---------------------------------------------------------

#ifdef STANDALONE

typedef struct serial_port_log_appender {
    log_appender appender;
    mem_allocator *allocator;
} serial_port_log_appender;

static void _serial_port_appender_append(log_appender *a, const char *line) {
    serial_port_log_appender *p = container_of(a, serial_port_log_appender, appender);
    
    while (*line) {
        // serial port write char (*line)
        line++;
    }
}

static const char *_serial_port_appender_peek_buffer(log_appender *a) {
    serial_port_log_appender *p = container_of(a, serial_port_log_appender, appender);
    return NULL;
}

static void _serial_port_appender_destroy(log_appender *a) {
    serial_port_log_appender *p = container_of(a, serial_port_log_appender, appender);
    p->allocator->release(p->allocator, p);
}

log_appender *create_serial_port_log_appender(mem_allocator *allocator, int port_no, int baud, int data_bits, char parity, int stop_bits) {
    serial_port_log_appender *p = allocator->allocate(allocator, sizeof(serial_port_log_appender));

    p->appender.append = _serial_port_appender_append;
    p->appender.peek_buffer = _serial_port_appender_peek_buffer;
    p->appender.destroy = _serial_port_appender_destroy;

    // initialize serial port here.

    return &p->appender;
}

#endif

// ---------------------------------------------------------

#ifdef HOSTED

typedef struct stderr_log_appender {
    log_appender appender;
    mem_allocator *allocator;
} stderr_log_appender;

static void _stderr_appender_append(log_appender *a, const char *line) {
    stderr_log_appender *p = container_of(a, stderr_log_appender, appender);
    fwrite(line, 1, strlen(line), stderr);
    fflush(stderr);
}

static const char *_stderr_appender_peek_buffer(log_appender *a) {
    stderr_log_appender *p = container_of(a, stderr_log_appender, appender);
    return NULL;
}

static void _stderr_appender_destroy(log_appender *a) {
    stderr_log_appender *p = container_of(a, stderr_log_appender, appender);
    p->allocator->release(p->allocator, p);
}

log_appender *create_stderr_log_appender(mem_allocator *allocator) {
    stderr_log_appender *p = allocator->allocate(allocator, sizeof(stderr_log_appender));

    p->appender.append = _stderr_appender_append;
    p->appender.peek_buffer = _stderr_appender_peek_buffer;
    p->appender.destroy = _stderr_appender_destroy;

    return &p->appender;
}
#endif // HOSTED

// ---------------------------------------------------------

#ifdef HOSTED

typedef struct text_file_log_appender {
    log_appender appender;
    mem_allocator *allocator;

    FILE *file;
} text_file_log_appender;

static void _text_file_appender_append(log_appender *a, const char *line) {
    text_file_log_appender *p = container_of(a, text_file_log_appender, appender);
    if (p->file != NULL) {
        fwrite(line, 1, strlen(line), p->file);
        fflush(p->file);
    }
}

static const char *_text_file_appender_peek_buffer(log_appender *a) {
    text_file_log_appender *p = container_of(a, text_file_log_appender, appender);
    return NULL;
}

static void _text_file_appender_destroy(log_appender *a) {
    text_file_log_appender *p = container_of(a, text_file_log_appender, appender);
    if (p->file != NULL) {
        fclose(p->file);
    }
    p->allocator->release(p->allocator, p);
}

log_appender *create_text_file_log_appender(mem_allocator *allocator, const char *filename) {
    text_file_log_appender *p = allocator->allocate(allocator, sizeof(text_file_log_appender));

    p->appender.append = _text_file_appender_append;
    p->appender.peek_buffer = _text_file_appender_peek_buffer;
    p->appender.destroy = _text_file_appender_destroy;

    p->file = fopen(filename, "a");

    return &p->appender;
}

#endif // HOSTED

// ---------------------------------------------------------
