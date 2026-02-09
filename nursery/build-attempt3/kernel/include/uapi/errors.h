#ifndef _ERRORS_H
#define _ERRORS_H


// typical unix / linux philosophy, given that it's easy for a function to return an int.
// zero:      success
// negative:  error
// positive:  some value

// this enum allows for better function signature, and clear indication of the return value
// typical example is readdir() which may return 0 for reading nothing, or ERR_EOF
typedef enum error_t {
    OK   = 0,
    ERR_NOT_FOUND            =  -1,
    ERR_NO_MORE_CONTENT      =  -2,   // for functions that read requential content
    ERR_PARTIAL_CONTENT_ONLY =  -3,   // reading dirs etc, means loading next sector is needed
    ERR_NOT_SUPPORTED        =  -4,
    ERR_NOT_IMPLEMENTED      =  -5,
    ERR_NOT_PERMITTED        =  -6,
    ERR_BAD_ARGUMENT         =  -7,
    ERR_BAD_VALUE            =  -8,
    ERR_ALREADY_EXISTS       =  -9,
    ERR_NO_DEVICE            = -10,
    ERR_NO_PARTITION         = -11,
    ERR_NO_DRIVER_FOUND      = -12,
    ERR_NOT_A_DIRECTORY      = -13,
    ERR_NOT_A_FILE           = -14,
    ERR_NO_SPACE_LEFT        = -15,
    ERR_NO_FS_MOUNTED        = -16,
    ERR_DIR_NOT_EMPTY        = -17,
    ERR_DIR_HAS_MOUNT        = -18,
    ERR_NO_RUNNING_PROCESS   = -19,
    ERR_READING_FILE         = -20,
    ERR_WRITING_FILE         = -21,
    ERR_HANDLES_EXHAUSTED    = -22,
    ERR_EOF                  = -23,
    ERR_NAME_TOO_LONG        = -24,
    ERR_CORRUPTION_DETECTED  = -25,
    ERR_CONTAINER_FULL       = -26,
    ERR_OVERFLOWN            = -27,
    ERR_UNDERFLOW            = -28,
    ERR_BUSY                 = -29,
    ERR_INVALID_ARGS         = -30,
    ERR_NO_MEMORY            = -31,
    ERR_IO_ERROR             = -32,
} error_t;



#endif