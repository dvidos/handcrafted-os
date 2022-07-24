#include <string.h>


// copies from offset onwards to buffer, advances offset
// ignores leading and trainling slashes
// returns bytes extracted
int get_next_path_part(char *path, int *offset, char *buffer) {
    char *start = path + *offset;
    if (*start == '/') {
        start++;
        (*offset)++;
    }
    if (*start == '\0') {
        buffer[0] = '\0';
        return 0;
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
    return strlen(buffer);
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

// gets the path part of index n (zero based)
// returns length of part copied - 0 if part not found
int get_n_index_path_part(char *path, int n, char *buffer) {
    int offset = 0;
    while (n-- >= 0) {
        int bytes = get_next_path_part(path, &offset, buffer);
        if (bytes == 0)
            return 0;            
    }
    
    return strlen(buffer);
}

