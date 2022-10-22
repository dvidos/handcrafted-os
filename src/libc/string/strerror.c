#include <errors.h>

// caller is supposed to free the duplicate
char *strerror(int err) {
    // avoiding an array to avoid 
    switch (err) {
        case NO_ERROR: return "NO_ERROR";
        case ERR_NOT_FOUND: return "NOT_FOUND";
        case ERR_NO_MORE_CONTENT: return "NO_MORE_CONTENT";
        case ERR_PARTIAL_CONTENT_ONLY: return "PARTIAL_CONTENT_ONLY";
        case ERR_NOT_SUPPORTED: return "NOT_SUPPORTED";
        case ERR_NOT_IMPLEMENTED: return "NOT_IMPLEMENTED";
        case ERR_BAD_ARGUMENT: return "BAD_ARGUMENT";
        case ERR_BAD_VALUE: return "BAD_VALUE";
        case ERR_NO_DEVICE: return "NO_DEVICE";
        case ERR_NO_PARTITION: return "NO_PARTITION";
        case ERR_NO_DRIVER_FOUND: return "NO_DRIVER_FOUND";
        case ERR_NOT_A_DIRECTORY: return "NOT_A_DIRECTORY";
        case ERR_NOT_A_FILE: return "NOT_A_FILE";
        case ERR_NO_SPACE_LEFT: return "NO_SPACE_LEFT";
        case ERR_NO_FS_MOUNTED: return "NO_FS_MOUNTED";
        case ERR_DIR_NOT_EMPTY: return "DIR_NOT_EMPTY";
        case ERR_NO_RUNNING_PROCESS: return "NO_RUNNING_PROCESS";
        case ERR_READING_FILE: return "READING_FILE";
        case ERR_WRITING_FILE: return "WRITING_FILE";
        case ERR_HANDLES_EXHAUSTED: return "HANDLES_EXHAUSTED";
    }

    return "(unknown error)";
}

