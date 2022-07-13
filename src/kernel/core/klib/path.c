#include <klib/string.h>
#include <errors.h>
#include <klog.h>



int get_next_path_part(char *path, int *offset, char *buffer) {
    klog_trace("get_next_path_part(path=\"%s\", offset=%d)", path, *offset);
    char *start = path + *offset;
    if (*start == '/') {
        start++;
        (*offset)++;
    }
    if (*start == '\0') {
        buffer[0] = '\0';
        return ERR_NO_MORE_CONTENT;
    }

    char *end = strchr(start, '/');
    if (end == NULL)
    {
        // strcpy() copies the zero terminator as well
        strcpy(buffer, start);
    }
    else
    {
        int length = end - start;
        memcpy(buffer, start, length);
        buffer[length] = '\0';
    }
    (*offset) += strlen(buffer);
    return SUCCESS;
}

