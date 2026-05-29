#pragma once


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
    ERR_NOT_INITIALIZED      =  -7,
    ERR_BAD_ARGUMENT         =  -8,
    ERR_BAD_VALUE            =  -9,
    ERR_ALREADY_EXISTS       = -10,
    ERR_NO_DEVICE            = -11,
    ERR_NO_PARTITION         = -12,
    ERR_NO_DRIVER_FOUND      = -13,
    ERR_NOT_A_DIRECTORY      = -14,
    ERR_NOT_A_FILE           = -15,
    ERR_NO_SPACE_LEFT        = -16,
    ERR_NO_FS_MOUNTED        = -17,
    ERR_DIR_NOT_EMPTY        = -18,
    ERR_DIR_HAS_MOUNT        = -19,
    ERR_NO_RUNNING_PROCESS   = -20,
    ERR_READING_FILE         = -21,
    ERR_WRITING_FILE         = -22,
    ERR_HANDLES_EXHAUSTED    = -23,
    ERR_EOF                  = -24,
    ERR_NAME_TOO_LONG        = -25,
    ERR_CORRUPTION_DETECTED  = -26,
    ERR_CONTAINER_FULL       = -27,
    ERR_OVERFLOWN            = -28,
    ERR_UNDERFLOW            = -29,
    ERR_BUSY                 = -30,
    ERR_INVALID_ARGS         = -31,
    ERR_NO_MEMORY            = -32,
    ERR_IO_ERROR             = -33,
    ERR_BAD_FILE             = -34,
    ERR_NO_CHILDREN          = -35,
    ERR_AGAIN                = -36,
    ERR_ACCESS_DENIED        = -37,
    ERR_INTERRUPTED          = -38,
    ERR_TOO_LONG             = -39,
    ERR_BAD_EXECUTABLE       = -40,
    ERR_BAD_ADDRESS          = -41,
    ERR_BLOCK_DEVICE_NEEDED  = -42,
    ERR_IS_A_DIRECTORY       = -43,
    ERR_TOO_MANY_OPEN_FILES  = -44,
    ERR_NOT_A_TTY            = -45,
    ERR_FILE_TOO_LARGE       = -46,
    ERR_READ_ONLY_SYSTEM     = -47,
    ERR_BROKEN_PIPE          = -48,
    ERR_OUT_OF_RANGE         = -49,
    ERR_ILLEGAL_SEEK         = -50,
    
    // ATA controller
    ERR_IDE_DEVICE_FAULT             = -101,
    ERR_IDE_STATUS_ERROR             = -102,
    ERR_IDE_NO_DATA_REQ              = -103,
    ERR_IDE_ADDR_MARK_NOT_FOUND      = -104,
    ERR_IDE_NO_MEDIA                 = -105,
    ERR_IDE_CMD_ABORTED              = -106,
    ERR_IDE_ID_MARK_NOT_FOUND        = -107,
    ERR_IDE_UNCORRECTABLE_DATA_ERROR = -108,
    ERR_IDE_BAD_SECTORS              = -109,
    ERR_IDE_DRIVE_NOT_FOUND          = -110,
    ERR_IDE_INVALID_ADDRESS          = -111,
    ERR_IDE_READ_ONLY                = -112,

    // SATA controller
    ERR_SATA_NO_CMD_SLOT             = -121,
    ERR_SATA_PORT_HUNG_BSY           = -122,
    ERR_SATA_TASK_FILE_ERROR         = -123,


} error_t;
