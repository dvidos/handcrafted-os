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


// returns how many parts in the path. leading and trailing '/' are ignored
int count_path_parts(char *path) {

    // find how many times we have a slash followed by a letter or something
    // this will exclude the last slash
    // don't require a slash in the beginning

    int len = strlen(path);
    if (len == 0 || (len == 1 && *path == '/'))
        return 0;

    // skip the first possible separator
    if (*path == '/') {
        path++;
        len--;
    }

    // we are either at end or at the start of a name after a slash
    int count = 1;
    while (len > 0) {
        // look for the next separator
        char *sep = strchr(path, '/');

        // if not found, or it's the last char, we are done.
        if (sep == NULL || *(sep + 1) == '\0')
            break;

        count++;
        int distance = sep - path; // include the separator
        path += distance + 1;
        len -= distance + 1;
    }

    return count;
}

// gets the path part of index n
int get_n_index_path_part(char *path, int n, char *buffer) {
    int offset = 0;
    while (n-- >= 0) {
        int err = get_next_path_part(path, &offset, buffer);
        if (err == ERR_NO_MORE_CONTENT)
            return ERR_NOT_FOUND;
        if (err)
            return err;
    }
    
    return SUCCESS;
}