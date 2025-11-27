#include "internal.h"


// returns the first name: "usr/sbin/ls" --> "usr" (buffer assumed at length)
static void path_get_first_part(const char *path, char *filename_buffer, int filename_buffer_size) {
    char *slash = strchr(path, '/');

    int length = (slash == NULL) ? strlen(path) : slash - path;
    if (length > filename_buffer_size - 1)
        length = filename_buffer_size - 1;
    
    strncpy(filename_buffer, path, filename_buffer_size);
    filename_buffer[length] = 0;
}


// returns just the last part (e.g. "sh" for "/bin/sh")
static const char *path_get_last_part(const char *path) {
    const char *separator = strrchr(path, '/');
    return separator == NULL ? path : separator + 1;
}
