#include "../libc_internal.h"
#include <string.h> // Not strictly needed for this implementation, but keeping for consistency
#include <errno.h>  // For E* macros
#include <stddef.h> // For size_t, which might be implicitly used by some headers

static char unknown[32];

char *strerror(int errnum) {
    switch ((error_t)errnum) {
        case OK:                               return "Success";
        case ERR_NOT_FOUND:                    return "Not found";
        case ERR_NO_MORE_CONTENT:              return "No more content";
        case ERR_PARTIAL_CONTENT_ONLY:         return "Partial content only";
        case ERR_NOT_SUPPORTED:                return "Not supported";
        case ERR_NOT_IMPLEMENTED:              return "Not implemented";
        case ERR_NOT_PERMITTED:                return "Not permitted";
        case ERR_NOT_INITIALIZED:              return "Not initialized";
        case ERR_BAD_ARGUMENT:                 return "Bad argument";
        case ERR_BAD_VALUE:                    return "Bad value";
        case ERR_ALREADY_EXISTS:               return "Already exists";
        case ERR_NO_DEVICE:                    return "No device";
        case ERR_NO_PARTITION:                 return "No partition";
        case ERR_NO_DRIVER_FOUND:              return "No driver found";
        case ERR_NOT_A_DIRECTORY:              return "Not a directory";
        case ERR_NOT_A_FILE:                   return "Not a file";
        case ERR_NO_SPACE_LEFT:                return "No space left";
        case ERR_NO_FS_MOUNTED:                return "No filesystem mounted";
        case ERR_DIR_NOT_EMPTY:                return "Dir not empty";
        case ERR_DIR_HAS_MOUNT:                return "Dir has mount";
        case ERR_NO_RUNNING_PROCESS:           return "No running process";
        case ERR_READING_FILE:                 return "Reading file";
        case ERR_WRITING_FILE:                 return "Writing file";
        case ERR_HANDLES_EXHAUSTED:            return "Handles exhausted";
        case ERR_EOF:                          return "End of file";
        case ERR_NAME_TOO_LONG:                return "Name too long";
        case ERR_CORRUPTION_DETECTED:          return "Corruption detected";
        case ERR_CONTAINER_FULL:               return "Container full";
        case ERR_OVERFLOWN:                    return "Overflown";
        case ERR_UNDERFLOW:                    return "Underflow";
        case ERR_BUSY:                         return "Busy";
        case ERR_INVALID_ARGS:                 return "Invalid args";
        case ERR_NO_MEMORY:                    return "No memory";
        case ERR_IO_ERROR:                     return "Io error";
        case ERR_BAD_FILE:                     return "Bad file";
        case ERR_NO_CHILDREN:                  return "No children";
        case ERR_AGAIN:                        return "Try again";
        case ERR_ACCESS_DENIED:                return "Access denied";
        case ERR_INTERRUPTED:                  return "Interrupted";
        case ERR_TOO_LONG:                     return "Too long";
        case ERR_BAD_EXECUTABLE:               return "Bad executable";
        case ERR_BAD_ADDRESS:                  return "Bad address";
        case ERR_BLOCK_DEVICE_NEEDED:          return "Block device needed";
        case ERR_IS_A_DIRECTORY:               return "Is a directory";
        case ERR_TOO_MANY_OPEN_FILES:          return "Too many open files";
        case ERR_NOT_A_TTY:                    return "Not a TTY";
        case ERR_FILE_TOO_LARGE:               return "File too large";
        case ERR_READ_ONLY_SYSTEM:             return "Read only system";
        case ERR_BROKEN_PIPE:                  return "Broken pipe";
        case ERR_OUT_OF_RANGE:                 return "Out of range";
        case ERR_IDE_DEVICE_FAULT:             return "IDE device fault";
        case ERR_IDE_STATUS_ERROR:             return "IDE status error";
        case ERR_IDE_NO_DATA_REQ:              return "IDE no data req";
        case ERR_IDE_ADDR_MARK_NOT_FOUND:      return "IDE addr mark not found";
        case ERR_IDE_NO_MEDIA:                 return "IDE no media";
        case ERR_IDE_CMD_ABORTED:              return "IDE cmd aborted";
        case ERR_IDE_ID_MARK_NOT_FOUND:        return "IDE id mark not found";
        case ERR_IDE_UNCORRECTABLE_DATA_ERROR: return "IDE uncorrectable data error";
        case ERR_IDE_BAD_SECTORS:              return "IDE bad sectors";
        case ERR_IDE_DRIVE_NOT_FOUND:          return "IDE drive not found";
        case ERR_IDE_INVALID_ADDRESS:          return "IDE invalid address";
        case ERR_IDE_READ_ONLY:                return "IDE read only";
        case ERR_SATA_NO_CMD_SLOT:             return "SATA no cmd slot";
        case ERR_SATA_PORT_HUNG_BSY:           return "SATA port hung bsy";
        case ERR_SATA_TASK_FILE_ERROR:         return "SATA task file error";
    }

    snprintf(unknown, sizeof(unknown), "Unknown error %d", errnum);
    return unknown;
}
