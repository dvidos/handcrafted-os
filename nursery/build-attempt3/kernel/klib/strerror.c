#include "../include/uapi/errors.h"
#include "../klib/string.h"

#define CASE(x)   case x: return #x

static char unknown_str_buffer[32];

const char *strerror(error_t err) {
    
    switch (err) {
        CASE(OK);
        CASE(ERR_NOT_FOUND);
        CASE(ERR_NO_MORE_CONTENT);
        CASE(ERR_PARTIAL_CONTENT_ONLY);
        CASE(ERR_NOT_SUPPORTED);
        CASE(ERR_NOT_IMPLEMENTED);
        CASE(ERR_NOT_PERMITTED);
        CASE(ERR_NOT_INITIALIZED);
        CASE(ERR_BAD_ARGUMENT);
        CASE(ERR_BAD_VALUE);
        CASE(ERR_ALREADY_EXISTS);
        CASE(ERR_NO_DEVICE);
        CASE(ERR_NO_PARTITION);
        CASE(ERR_NO_DRIVER_FOUND);
        CASE(ERR_NOT_A_DIRECTORY);
        CASE(ERR_NOT_A_FILE);
        CASE(ERR_NO_SPACE_LEFT);
        CASE(ERR_NO_FS_MOUNTED);
        CASE(ERR_DIR_NOT_EMPTY);
        CASE(ERR_DIR_HAS_MOUNT);
        CASE(ERR_NO_RUNNING_PROCESS);
        CASE(ERR_READING_FILE);
        CASE(ERR_WRITING_FILE);
        CASE(ERR_HANDLES_EXHAUSTED);
        CASE(ERR_EOF);
        CASE(ERR_NAME_TOO_LONG);
        CASE(ERR_CORRUPTION_DETECTED);
        CASE(ERR_CONTAINER_FULL);
        CASE(ERR_OVERFLOWN);
        CASE(ERR_UNDERFLOW);
        CASE(ERR_BUSY);
        CASE(ERR_INVALID_ARGS);
        CASE(ERR_NO_MEMORY);
        CASE(ERR_IO_ERROR);
        CASE(ERR_BAD_FILE);
        CASE(ERR_NO_CHILDREN);
        CASE(ERR_AGAIN);
        CASE(ERR_IDE_DEVICE_FAULT);
        CASE(ERR_IDE_STATUS_ERROR);
        CASE(ERR_IDE_NO_DATA_REQ);
        CASE(ERR_IDE_ADDR_MARK_NOT_FOUND);
        CASE(ERR_IDE_NO_MEDIA);
        CASE(ERR_IDE_CMD_ABORTED);
        CASE(ERR_IDE_ID_MARK_NOT_FOUND);
        CASE(ERR_IDE_UNCORRECTABLE_DATA_ERROR);
        CASE(ERR_IDE_BAD_SECTORS);
        CASE(ERR_IDE_DRIVE_NOT_FOUND);
        CASE(ERR_IDE_INVALID_ADDRESS);
        CASE(ERR_IDE_READ_ONLY);
        CASE(ERR_SATA_NO_CMD_SLOT);
        CASE(ERR_SATA_PORT_HUNG_BSY);
        CASE(ERR_SATA_TASK_FILE_ERROR);
    }

    sprintfn(unknown_str_buffer, sizeof(unknown_str_buffer), "(unknown error_t: %u)", err);
    return unknown_str_buffer;
}
