#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h> // For ftruncate
#include <errno.h>  // For errno and strerror
#include <sys/stat.h>
#include <dirent.h>

// External modules
#include "file_sector_device.h"
#include "partition_sector_device.h"
#include "utils.h" // For parse_size, hexdump_with_folding, and error
#include "command_parser.h" // For command parsing structures and functions

// Dependencies for simple_filesystem
#include "../dependencies/mem_allocator.h"
#include "../dependencies/clock_device.h"
#include "../simple_filesystem.h"


// --- Forward Declarations of Command Execution Functions ---

static int execute_create(command_options *opts, int argc, char *argv[]);
static int execute_wrsect(command_options *opts, int argc, char *argv[]);
static int execute_rdsect(command_options *opts, int argc, char *argv[]);
static int execute_wrpart(command_options *opts, int argc, char *argv[]);
static int execute_rdpart(command_options *opts, int argc, char *argv[]);
static int execute_mkfs(command_options *opts, int argc, char *argv[]);
static int execute_info(command_options *opts, int argc, char *argv[]);
static int execute_ls(command_options *opts, int argc, char *argv[]);
static int execute_mkdir(command_options *opts, int argc, char *argv[]);
static int execute_import(command_options *opts, int argc, char *argv[]);
static int execute_import_all(command_options *opts, int argc, char *argv[]);
static int execute_export(command_options *opts, int argc, char *argv[]);
static int execute_rm(command_options *opts, int argc, char *argv[]);
static int execute_help(command_options *opts, int argc, char *argv[]);


// --- Global Commands Array ---

// Forward declaration for print_general_help (used in execute_help and dispatcher)
void print_general_help(); 

const command_config commands[] = { // Not static so it can be passed to command_parser
    {"create", "Create a new disk image", execute_create, 
        (const option_config[]){
            {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING, true},
            {"--size", 's', "Size of the volume (e.g., 20M)", true, "size", OPT_STRING, true},
            {NULL, 0, NULL, false, NULL, 0, false} // Terminator
        }, NULL
    },
    {"wrsect", "Write sectors from a file", execute_wrsect, 
        (const option_config[]){
            {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING, true},
            {"--sector", 's', "Starting sector number to write to", true, "sector", OPT_INT, true},
            {"--file", 'f', "File to read data from", true, "file", OPT_STRING, true},
            {"--count", 'c', "Number of sectors to write (default: 1)", true, "count", OPT_INT, false},
            {NULL, 0, NULL, false, NULL, 0, false} // Terminator
        }, NULL
    },
    {"rdsect", "Read sectors to a file or stdout", execute_rdsect, 
        (const option_config[]){
            {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING, true},
            {"--sector", 's', "Starting sector number to read from", true, "sector", OPT_INT, true},
            {"--file", 'f', "File to write data to (optional)", true, "file", OPT_STRING, false},
            {"--count", 'c', "Number of sectors to read (default: 1)", true, "count", OPT_INT, false},
            {NULL, 0, NULL, false, NULL, 0, false} // Terminator
        }, NULL
    },
    {"wrpart", "Write a partition table entry", execute_wrpart, 
        (const option_config[]){
            {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING, true},
            {"--entry", 'e', "Partition entry number (1-4)", true, "entry", OPT_INT, true},
            {"--first-sector", 'f', "First sector LBA", true, "first_sector", OPT_INT, true},
            {"--sector-count", 'c', "Number of sectors in the partition", true, "sector_count", OPT_INT, true},
            {"--type", 't', "Partition type (byte value, can be hex)", true, "type", OPT_STRING, true},
            {"--bootable", 'b', "Bootable flag (1 or 0)", true, "bootable", OPT_INT, true},
            {NULL, 0, NULL, false, NULL, 0, false} // Terminator
        }, NULL
    },
    {"rdpart", "Read a partition table entry", execute_rdpart, 
        (const option_config[]){
            {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING, true},
            {"--entry", 'e', "Partition entry number (1-4)", true, "entry", OPT_INT, true},
            {NULL, 0, NULL, false, NULL, 0, false} // Terminator
        }, NULL
    },
    {"mkfs", "Create a filesystem on the disk image", execute_mkfs, 
        (const option_config[]){
            {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING, true},
            {"--start-sector", 's', "Sector where the filesystem starts", true, "start", OPT_INT, true},
            {"--label", 'l', "Volume label", true, "label", OPT_STRING, true},
            {NULL, 0, NULL, false, NULL, 0, false} // Terminator
        }, NULL
    },
    {"info", "Display filesystem information", execute_info, 
        (const option_config[]){
            {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING, true},
            {"--start-sector", 's', "Sector where the filesystem starts", true, "start", OPT_INT, true},
            {NULL, 0, NULL, false, NULL, 0, false} // Terminator
        }, NULL
    },
    {"ls", "List files in a directory", execute_ls, 
        (const option_config[]){
            {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING, true},
            {"--start-sector", 's', "Sector where the filesystem starts", true, "start", OPT_INT, true},
            {NULL, 0, NULL, false, NULL, 0, false} // Terminator
        }, 
        (const arg_config[]){
            {"[path]", "The path to list. Defaults to the root directory.", true},
            {NULL, NULL, false}
        }
    },
    {"mkdir", "Create a directory", execute_mkdir, 
        (const option_config[]){
            {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING, true},
            {"--start-sector", 's', "Sector where the filesystem starts", true, "start", OPT_INT, true},
            {NULL, 0, NULL, false, NULL, 0, false} // Terminator
        }, 
        (const arg_config[]){
            {"<path>", "The full path of the directory to create.", false},
            {NULL, NULL, false}
        }
    },
    {"import", "Import a file from host to the filesystem", execute_import, 
        (const option_config[]){
            {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING, true},
            {"--start-sector", 's', "Sector where the filesystem starts", true, "start", OPT_INT, true},
            {NULL, 0, NULL, false, NULL, 0, false} // Terminator
        }, 
        (const arg_config[]){
            {"<host_file>", "The path to the file on the host machine.", false},
            {"<sfs_path>", "The destination path within the SFS image.", false},
            {NULL, NULL, false}
        }
    },
    {"import-all", "Import a directory from host to the filesystem recursively", execute_import_all, 
        (const option_config[]){
            {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING, true},
            {"--start-sector", 's', "Sector where the filesystem starts", true, "start", OPT_INT, true},
            {NULL, 0, NULL, false, NULL, 0, false} // Terminator
        }, 
        (const arg_config[]){
            {"<host_dir>", "The path to the directory on the host machine.", false},
            {"[sfs_dir]", "The destination directory within the SFS image. Defaults to root.", true},
            {NULL, NULL, false}
        }
    },
    {"export", "Export a file from the filesystem to host", execute_export, 
        (const option_config[]){
            {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING, true},
            {"--start-sector", 's', "Sector where the filesystem starts", true, "start", OPT_INT, true},
            {NULL, 0, NULL, false, NULL, 0, false} // Terminator
        }, 
        (const arg_config[]){
            {"<sfs_path>", "The path of the file within the SFS image.", false},
            {"<host_file>", "The destination path on the host machine.", false},
            {NULL, NULL, false}
        }
    },
    {"rm", "Remove a file or directory", execute_rm, 
        (const option_config[]){
            {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING, true},
            {"--start-sector", 's', "Sector where the filesystem starts", true, "start", OPT_INT, true},
            {"--dir", 'd', "Remove a directory (default: file)", false, "is_dir", OPT_BOOL, false},
            {NULL, 0, NULL, false, NULL, 0, false} // Terminator
        }, 
        (const arg_config[]){
            {"<sfs_path>", "The path to the file or directory to remove.", false},
            {NULL, NULL, false}
        }
    },
    {"help", "Display help for commands", execute_help, NULL, 
        (const arg_config[]){
            {"<command>", "The command to get help for.", false},
            {NULL, NULL, false}
        }
    },
    {NULL, NULL, NULL, NULL, NULL} // Terminator
};
const size_t NUM_COMMANDS = (sizeof(commands) / sizeof(commands[0]) -1); // -1 because of NULL terminator

// Temporary definition for SFS_O_WRONLY if not found in simple_filesystem.h
#ifndef SFS_O_WRONLY
#define SFS_O_WRONLY 1 // Temporary definition for compilation if not found
#endif
// Temporary definition for SFS_O_RDONLY if not found in simple_filesystem.h
#ifndef SFS_O_RDONLY
#define SFS_O_RDONLY 0 // Assuming 0 for read-only if no other flag is specified
#endif


// --- Command Implementations ---

typedef struct {
    long start_sector;
    sector_device *base_dev;
    sector_device *part_dev;
    mem_allocator *mem_alloc;
    clock_device *clock_dev;
    simple_filesystem *sfs_instance;
    FILE *host_fp;
    sfs_handle *sfs_file_handle;
    uint8_t *buffer;
} sfs_runtime_context;

#define MOUNT_READWRITE   0
#define MOUNT_READONLY    1
#define MOUNT_NONE        2

// --- Context-aware command execution wrapper ---

// Forward declarations for context functions
static int setup_sfs_context(const char* image_file, long start_sector, int mount_type, sfs_runtime_context *context);
static void teardown_sfs_context(sfs_runtime_context *context);

// Define a function pointer type for the core logic of SFS commands
typedef int (*sfs_command_logic)(sfs_runtime_context *context, command_options *opts, int argc, char *argv[]);

// Wrapper function to handle SFS context setup and teardown
static int with_sfs_context(command_options *opts, int argc, char *argv[], int mount_type, sfs_command_logic logic) {
    const char *image_file = get_str_option(opts, "image");
    if (!image_file) return error("Missing required --image argument.");
    
    long start_sector = get_int_option(opts, "start", -1);
    if (start_sector == -1) return error("Missing required --start-sector argument.");

    sfs_runtime_context context;
    if (setup_sfs_context(image_file, start_sector, mount_type, &context) != 0) {
        teardown_sfs_context(&context);
        return -1;
    }

    int result = logic(&context, opts, argc, argv);

    teardown_sfs_context(&context);
    return result;
}

static inline uint32_t read_le32(const uint8_t *buffer) {
    return buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
}

static inline void write_le32(uint8_t *buffer, uint32_t value) {
    buffer[0] = value & 0xFF;
    buffer[1] = (value >> 8) & 0xFF;
    buffer[2] = (value >> 16) & 0xFF;
    buffer[3] = (value >> 24) & 0xFF;
}



// Function to initialize and set up the SFS context
static int setup_sfs_context(const char* image_file, long start_sector, int mount_type, sfs_runtime_context *context) {
    // Initialize all pointers to NULL to ensure cleanup works safely
    context->host_fp = NULL;
    context->base_dev = NULL;
    context->part_dev = NULL;
    context->mem_alloc = NULL;
    context->clock_dev = NULL;
    context->sfs_instance = NULL;
    context->sfs_file_handle = NULL;
    context->buffer = NULL;
    context->start_sector = start_sector; // Store start_sector in context

    // 1. Create file_sector_device
    context->base_dev = new_file_sector_device(image_file);
    if (!context->base_dev) {
        return error("Could not open image file '%s'.", image_file);
    }

    uint32_t total_image_sectors = context->base_dev->get_sector_count(context->base_dev);
    if (start_sector >= total_image_sectors) {
        return error("Start sector %ld is beyond total image sectors %u.", start_sector, total_image_sectors);
    }
    uint32_t partition_sector_count = total_image_sectors - start_sector;
    if (partition_sector_count == 0) {
        return error("Partition has zero sectors.");
    }

    // 2. Create partition_sector_device
    context->part_dev = new_partition_sector_device(context->base_dev, start_sector, partition_sector_count);
    if (!context->part_dev) {
        return error("Could not create partition device.");
    }

    // 3. Create mem_allocator and clock_device
    context->mem_alloc = new_malloc_based_mem_allocator();
    if (!context->mem_alloc) {
        return error("Could not create memory allocator.");
    }
    context->clock_dev = new_rtc_based_clock_device();
    if (!context->clock_dev) {
        return error("Could not create clock device.");
    }

    // 4. Create simple_filesystem instance
    context->sfs_instance = new_simple_filesystem(context->mem_alloc, context->part_dev, context->clock_dev);
    if (!context->sfs_instance) {
        return error("Could not create simple_filesystem instance.");
    }

    // 5. Mount Filesystem
    if (mount_type != MOUNT_NONE) {
        int err = context->sfs_instance->mount(context->sfs_instance, mount_type == MOUNT_READONLY ? 1 : 0);
        if (err)
            return error("Failed (%d) to mount filesystem on '%s' starting at sector %ld.", err, image_file, start_sector);
    }

    return 0; // Success
}

static void teardown_sfs_context(sfs_runtime_context *context) {
    if (context->sfs_file_handle) {
        if (context->sfs_instance) {
            context->sfs_instance->close(context->sfs_instance, context->sfs_file_handle);
        }
    }
    if (context->sfs_instance) {
        context->sfs_instance->unmount(context->sfs_instance);
    }
    if (context->host_fp) {
        fclose(context->host_fp);
    }
    if (context->buffer) {
        free(context->buffer);
    }
    // TODO: Add releases for clock_dev, mem_alloc, part_dev, base_dev when available
    // For now, free only if they have a dedicated release function which isn't present yet.
    // As per previous TODO comments, release functions for base_dev, part_dev, mem_alloc, clock_dev
    // are not yet implemented in the underlying libraries.
}


static int execute_create(command_options *opts, int argc, char *argv[]) {
    const char *image_file = get_str_option(opts, "image");
    if (image_file == NULL) return error("Missing required --image argument for 'create' command.");
    const char *size_str = get_str_option(opts, "size");
    if (size_str == NULL) return error("Missing required --size argument for 'create' command.");

    long size = parse_size(size_str);
    FILE *f = fopen(image_file, "w");
    if (!f) return error("Error opening file '%s' for create: %s", image_file, strerror(errno));

    if (ftruncate(fileno(f), size) != 0) {
        int err_val = errno; // Capture errno before any other call might change it
        fclose(f);
        return error("Error truncating file '%s': %s", image_file, strerror(err_val));
    }

    fclose(f);
    printf("Created image '%s' with size %ld bytes.\n", image_file, size);
    return 0;
}

static int execute_wrsect(command_options *opts, int argc, char *argv[]) {
    const char *image_file = get_str_option(opts, "image");
    if (image_file == NULL) return error("Missing required --image argument for 'wrsect' command.");
    long start_sector = get_int_option(opts, "sector", -1);
    long count = get_int_option(opts, "count", 1);
    const char *file_str = get_str_option(opts, "file");

    if (start_sector == -1 || file_str == NULL) return error("Missing required --sector or --file argument for 'wrsect' command.");
    if (count <= 0) return error("--count must be a positive number.");

    FILE *src_file = fopen(file_str, "r");
    if (!src_file) return error("Error opening source file '%s': %s", file_str, strerror(errno));

    // Get actual size of the source file
    fseek(src_file, 0, SEEK_END);
    long source_file_actual_size = ftell(src_file);
    fseek(src_file, 0, SEEK_SET); // Rewind to the beginning

    sector_device *dev = new_file_sector_device(image_file);
    if (!dev) {
        fclose(src_file);
        return error("Could not open image file '%s'.", image_file);
    }

    uint32_t sector_size = dev->get_sector_size(dev);
    long total_bytes_to_write = count * sector_size;
    uint8_t *buffer = calloc(1, total_bytes_to_write);
    
    size_t actual_bytes_read = fread(buffer, 1, total_bytes_to_write, src_file);
    fclose(src_file);

    if (actual_bytes_read > 0 && actual_bytes_read < total_bytes_to_write) {
        printf("Warning: Source file '%s' (%zu bytes) is smaller than target write size (%ld bytes). Padding with zeros.\n", file_str, actual_bytes_read, total_bytes_to_write);
    } else if (actual_bytes_read == 0 && total_bytes_to_write > 0) {
        printf("Warning: Source file '%s' is empty, writing %ld zero bytes.\n", file_str, total_bytes_to_write);
    } else if (source_file_actual_size > total_bytes_to_write) {
        error("Warning: Source file '%s' (%zu bytes) is larger than target write size (%ld bytes). Truncating to %ld bytes.\n", file_str, actual_bytes_read, total_bytes_to_write, total_bytes_to_write);
    }


    for (int i = 0; i < count; i++) {
        if (dev->write_sector(dev, start_sector + i, buffer + (i * sector_size)) != 0) {
            free(buffer);
            return error("Error writing sector %ld to image '%s'.", start_sector + i, image_file);
        }
    }

    free(buffer);
    printf("Wrote %ld sectors (total %ld bytes) starting from sector %ld of '%s' from '%s'.\n", count, total_bytes_to_write, start_sector, image_file, file_str);
    return 0;
}

static int execute_rdsect(command_options *opts, int argc, char *argv[]) {
    const char *image_file = get_str_option(opts, "image");
    if (image_file == NULL) return error("Missing required --image argument for 'rdsect' command.");
    long start_sector = get_int_option(opts, "sector", -1);
    long count = get_int_option(opts, "count", 1);
    const char *file_str = get_str_option(opts, "file");

    if (start_sector == -1) {
        return error("Missing required --sector argument for 'rdsect' command.");
    }
    if (count <= 0) {
        return error("--count must be a positive number.");
    }

    sector_device *dev = new_file_sector_device(image_file);
    if (!dev) {
        return error("Could not open image file '%s'.", image_file);
    }

    uint32_t sector_size = dev->get_sector_size(dev);
    long total_bytes_to_read = count * sector_size;
    uint8_t *buffer = malloc(total_bytes_to_read);
    if (!buffer) return error("Failed to allocate memory for reading sectors.");
    
    for (int i = 0; i < count; i++) {
        if (dev->read_sector(dev, start_sector + i, buffer + (i * sector_size)) != 0) {
            free(buffer);            return error("Error reading sector %ld from image '%s'.", start_sector + i, image_file);
        }
    }

    if (file_str) {
        FILE *f = fopen(file_str, "w");
        if (!f) {
            int err_val = errno;
            free(buffer);            return error("Error opening output file '%s': %s", file_str, strerror(err_val));
        }
        fwrite(buffer, 1, total_bytes_to_read, f);
        fclose(f);
        printf("Read %ld sectors (total %ld bytes) starting from sector %ld from '%s' into '%s'.\n", count, total_bytes_to_read, start_sector, image_file, file_str);
    } else {
        printf("--- Hexdump of %ld sectors starting from %ld ---\n", count, start_sector);
        hexdump_with_folding(buffer, total_bytes_to_read, start_sector * sector_size);
        printf("--- End of Hexdump ---\n");
    }

    free(buffer);    return 0;
}

static int execute_wrpart(command_options *opts, int argc, char *argv[]) {
    const char *image_file = get_str_option(opts, "image");
    if (image_file == NULL) return error("Missing required --image argument for 'wrpart' command.");
    long entry = get_int_option(opts, "entry", -1);
    long first_sector = get_int_option(opts, "first_sector", -1);
    long sector_count = get_int_option(opts, "sector_count", -1);
    const char *type_str = get_str_option(opts, "type");
    long bootable = get_int_option(opts, "bootable", -1);

    if (entry < 1 || entry > 4) return error("Partition entry must be between 1 and 4.");
    if (first_sector < 0) return error("First sector must be a non-negative number.");
    if (sector_count <= 0) return error("Sector count must be positive.");
    if (type_str == NULL) return error("Missing required --type argument.");
    if (bootable != 0 && bootable != 1) return error("Bootable flag must be 0 or 1.");

    uint8_t type = (uint8_t)strtol(type_str, NULL, 0);

    sector_device *dev = new_file_sector_device(image_file);
    if (!dev) return error("Could not open image file '%s'.", image_file);

    uint32_t sector_size = dev->get_sector_size(dev);
    uint8_t *buffer = malloc(sector_size);
    if (!buffer) return error("Failed to allocate memory for reading sector.");

    if (dev->read_sector(dev, 0, buffer) != 0) {
        free(buffer);
        return error("Error reading MBR sector from image '%s'.", image_file);
    }

    uint8_t *part_entry = buffer + 0x1BE + ((entry - 1) * 16);

    // Clear the entry first
    memset(part_entry, 0, 16);

    part_entry[0] = (bootable == 1) ? 0x80 : 0x00;
    part_entry[4] = type;
    write_le32(part_entry + 8, (uint32_t)first_sector);
    write_le32(part_entry + 12, (uint32_t)sector_count);

    if (dev->write_sector(dev, 0, buffer) != 0) {
        free(buffer);
        return error("Error writing MBR sector to image '%s'.", image_file);
    }

    printf("Successfully wrote partition %ld to '%s'.\n", entry, image_file);

    free(buffer);
    return 0;
}

static int execute_rdpart(command_options *opts, int argc, char *argv[]) {
    const char *image_file = get_str_option(opts, "image");
    if (image_file == NULL) return error("Missing required --image argument for 'rdpart' command.");
    long entry = get_int_option(opts, "entry", -1);

    if (entry < 1 || entry > 4) {
        return error("Partition entry must be between 1 and 4.");
    }

    sector_device *dev = new_file_sector_device(image_file);
    if (!dev) return error("Could not open image file '%s'.", image_file);

    uint32_t sector_size = dev->get_sector_size(dev);
    uint8_t *buffer = malloc(sector_size);
    if (!buffer) {
        // dev->release(dev); // Assuming dev has a release method
        return error("Failed to allocate memory for reading sector.");
    }

    if (dev->read_sector(dev, 0, buffer) != 0) {
        free(buffer);
        // dev->release(dev);
        return error("Error reading MBR sector from image '%s'.", image_file);
    }

    // Partition table starts at 0x1BE
    const uint8_t *part_entry = buffer + 0x1BE + ((entry - 1) * 16);

    uint8_t bootable = part_entry[0];
    uint8_t type = part_entry[4];
    uint32_t first_sector = read_le32(part_entry + 8);
    uint32_t sector_count = read_le32(part_entry + 12);

    printf("Partition %ld:\n", entry);
    printf("  Bootable: %c\n", (bootable == 0x80) ? 'Y' : 'n');
    printf("  Type: 0x%02x\n", type);
    printf("  Start Sector: %u\n", first_sector);
    printf("  Sector Count: %u\n", sector_count);

    free(buffer);
    // dev->release(dev);
    return 0;
}

static int do_mkfs(sfs_runtime_context *context, command_options *opts, int argc, char *argv[]) {
    const char *label_str = get_str_option(opts, "label");
    if (label_str == NULL) {
        return error("Missing required --label argument for 'mkfs' command.");
    }
    if (strlen(label_str) > 31) {
        return error("Volume label must be 31 characters or less.");
    }

    int err = context->sfs_instance->mkfs(context->sfs_instance, (char*)label_str, DESIRED_BLOCK_AUTO);
    if (err) {
        return error("Failed (%d) to create filesystem.", err);
    }

    printf("Successfully created SFS filesystem with label '%s'.\n", label_str);
    return 0;
}

static int execute_mkfs(command_options *opts, int argc, char *argv[]) {
    return with_sfs_context(opts, argc, argv, MOUNT_NONE, do_mkfs);
}

static int do_info(sfs_runtime_context *context, command_options *opts, int argc, char *argv[]) {
    printf("Filesystem Information (partition at sector %ld):\n", context->start_sector);
    context->sfs_instance->dump_debug_info(context->sfs_instance, "Filesystem Info");
    return 0;
}

static int execute_info(command_options *opts, int argc, char *argv[]) {
    return with_sfs_context(opts, argc, argv, MOUNT_READONLY, do_info);
}

static int do_ls(sfs_runtime_context *context, command_options *opts, int argc, char *argv[]) {
    const char *path = (argc > 0) ? argv[0] : "/";

    sfs_handle *dir_handle = NULL;
    if (context->sfs_instance->open_dir(context->sfs_instance, (char*)path, &dir_handle) != 0) {
        return error("Could not open directory '%s'.", path);
    }
    context->sfs_file_handle = dir_handle; // For auto-cleanup

    sfs_dir_entry entry;
    printf("Contents of '%s':\n", path);
    while (context->sfs_instance->read_dir(context->sfs_instance, dir_handle, &entry) == 0) {
        if (entry.name[0] != '\0') {
            printf("  %s\n", entry.name);
        }
    }
    return 0;
}

static int execute_ls(command_options *opts, int argc, char *argv[]) {
    return with_sfs_context(opts, argc, argv, MOUNT_READONLY, do_ls);
}

static int do_mkdir(sfs_runtime_context *context, command_options *opts, int argc, char *argv[]) {
    if (argc < 1) {
        return error("Missing required path argument for 'mkdir' command.");
    }
    const char *path = argv[0];

    int err = context->sfs_instance->create(context->sfs_instance, (char*)path, 1 /* is_dir */);
    if (err) {
        return error("Failed (%d) to create directory '%s'.", err, path);
    }

    printf("Successfully created directory '%s'.\n", path);
    return 0;
}

static int execute_mkdir(command_options *opts, int argc, char *argv[]) {
    return with_sfs_context(opts, argc, argv, MOUNT_READWRITE, do_mkdir);
}

static int do_import(sfs_runtime_context *context, command_options *opts, int argc, char *argv[]) {
    if (argc < 2) {
        return error("Missing required arguments. Usage: import <host_file> <sfs_path>");
    }
    const char *source_host_file = argv[0];
    const char *destination_sfs_path = argv[1];

    context->host_fp = fopen(source_host_file, "rb");
    if (!context->host_fp) {
        return error("Error opening host file '%s': %s", source_host_file, strerror(errno));
    }

    if (context->sfs_instance->create(context->sfs_instance, (char*)destination_sfs_path, 0) != 0) {
        fprintf(stderr, "Warning: Failed to create SFS file '%s'. It may already exist.\n", destination_sfs_path);
    }

    if (context->sfs_instance->open(context->sfs_instance, (char*)destination_sfs_path, SFS_O_WRONLY, &context->sfs_file_handle) != 0) {
        return error("Failed to open SFS file '%s' for writing.", destination_sfs_path);
    }

    uint32_t sector_size = context->base_dev->get_sector_size(context->base_dev);
    context->buffer = malloc(sector_size);
    if (!context->buffer) {
        return error("Failed to allocate buffer for import operation.");
    }

    size_t bytes_read;
    long total_bytes_imported = 0;
    while ((bytes_read = fread(context->buffer, 1, sector_size, context->host_fp)) > 0) {
        int written_bytes = context->sfs_instance->write(context->sfs_instance, context->sfs_file_handle, context->buffer, bytes_read);
        if (written_bytes < 0 || (size_t)written_bytes != bytes_read) {
            return error("Error writing to SFS file '%s'.", destination_sfs_path);
        }
        total_bytes_imported += written_bytes;
    }

    if (ferror(context->host_fp)) {
        return error("Error reading from host file '%s'.", source_host_file);
    }

    printf("Successfully imported '%s' (host) to '%s' (SFS) with %ld bytes.\n", source_host_file, destination_sfs_path, total_bytes_imported);
    return 0;
}

static int execute_import(command_options *opts, int argc, char *argv[]) {
    return with_sfs_context(opts, argc, argv, MOUNT_READWRITE, do_import);
}

static int import_directory_recursive(sfs_runtime_context* context, const char* host_base_dir, const char* sfs_base_dir, int* dir_count, int* file_count, long* total_bytes) {
    DIR* dir = opendir(host_base_dir);
    if (!dir) {
        return error("Failed to open host directory '%s': %s", host_base_dir, strerror(errno));
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char host_path[1024];
        snprintf(host_path, sizeof(host_path), "%s/%s", host_base_dir, entry->d_name);

        char sfs_path[1024];
        snprintf(sfs_path, sizeof(sfs_path), "%s%s%s", sfs_base_dir, (strcmp(sfs_base_dir, "/") == 0 ? "" : "/"), entry->d_name);

        struct stat st;
        if (stat(host_path, &st) != 0) {
            error("Failed to stat '%s': %s", host_path, strerror(errno));
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (context->sfs_instance->create(context->sfs_instance, sfs_path, 1) == 0) {
                (*dir_count)++;
                import_directory_recursive(context, host_path, sfs_path, dir_count, file_count, total_bytes);
            } else {
                error("Failed to create SFS directory '%s'", sfs_path);
            }
        } else if (S_ISREG(st.st_mode)) {
            FILE* host_file = fopen(host_path, "rb");
            if (!host_file) {
                error("Failed to open host file '%s': %s", host_path, strerror(errno));
                continue;
            }

            if (context->sfs_instance->create(context->sfs_instance, sfs_path, 0) != 0) {
                error("Failed to create SFS file '%s'", sfs_path);
                fclose(host_file);
                continue;
            }

            sfs_handle* file_handle;
            if (context->sfs_instance->open(context->sfs_instance, sfs_path, SFS_O_WRONLY, &file_handle) != 0) {
                error("Failed to open SFS file '%s' for writing", sfs_path);
                fclose(host_file);
                continue;
            }

            uint32_t sector_size = context->base_dev->get_sector_size(context->base_dev);
            uint8_t* buffer = malloc(sector_size);
            if (!buffer) {
                error("Failed to allocate buffer for import.");
                fclose(host_file);
                context->sfs_instance->close(context->sfs_instance, file_handle);
                continue;
            }

            size_t bytes_read;
            while ((bytes_read = fread(buffer, 1, sector_size, host_file)) > 0) {
                int written_bytes = context->sfs_instance->write(context->sfs_instance, file_handle, buffer, bytes_read);
                if (written_bytes < 0 || (size_t)written_bytes != bytes_read) {
                    error("Error writing to SFS file '%s'", sfs_path);
                    break;
                }
                *total_bytes += written_bytes;
            }

            free(buffer);
            fclose(host_file);
            context->sfs_instance->close(context->sfs_instance, file_handle);
            (*file_count)++;
        }
    }

    closedir(dir);
    return 0;
}

static int do_import_all(sfs_runtime_context *context, command_options *opts, int argc, char *argv[]) {
    if (argc < 1) {
        return error("Missing required arguments. Usage: import-all <host_dir> [<sfs_dir>]");
    }
    const char *source_host_dir = argv[0];
    const char *destination_sfs_dir = (argc > 1) ? argv[1] : "/";

    int dir_count = 0;
    int file_count = 0;
    long total_bytes = 0;

    import_directory_recursive(context, source_host_dir, destination_sfs_dir, &dir_count, &file_count, &total_bytes);

    printf("Import summary:\n");
    printf("  Directories created: %d\n", dir_count);
    printf("  Files imported: %d\n", file_count);
    printf("  Total bytes imported: %ld\n", total_bytes);

    return 0;
}

static int execute_import_all(command_options *opts, int argc, char *argv[]) {
    return with_sfs_context(opts, argc, argv, MOUNT_READWRITE, do_import_all);
}

static int do_export(sfs_runtime_context *context, command_options *opts, int argc, char *argv[]) {
    if (argc < 2) {
        return error("Missing required arguments. Usage: export <sfs_path> <host_file>");
    }
    const char *source_sfs_path = argv[0];
    const char *destination_host_file = argv[1];

    if (context->sfs_instance->open(context->sfs_instance, (char*)source_sfs_path, SFS_O_RDONLY, &context->sfs_file_handle) != 0) {
        return error("Failed to open SFS file '%s' for reading.", source_sfs_path);
    }

    context->host_fp = fopen(destination_host_file, "wb");
    if (!context->host_fp) {
        return error("Error opening host file '%s' for writing: %s", destination_host_file, strerror(errno));
    }

    uint32_t sector_size = context->base_dev->get_sector_size(context->base_dev);
    context->buffer = malloc(sector_size);
    if (!context->buffer) {
        return error("Failed to allocate buffer for export operation.");
    }

    size_t bytes_read;
    long total_bytes_exported = 0;
    while ((bytes_read = context->sfs_instance->read(context->sfs_instance, context->sfs_file_handle, context->buffer, sector_size)) > 0) {
        size_t written_bytes = fwrite(context->buffer, 1, bytes_read, context->host_fp);
        if (written_bytes != bytes_read) {
            return error("Error writing to host file '%s'.", destination_host_file);
        }
        total_bytes_exported += bytes_read;
    }

    if ((int)bytes_read == -1) {
        return error("Error reading from SFS file '%s'.", source_sfs_path);
    }

    printf("Successfully exported '%s' (SFS) to '%s' (host) with %ld bytes.\n", source_sfs_path, destination_host_file, total_bytes_exported);
    return 0;
}

static int execute_export(command_options *opts, int argc, char *argv[]) {
    return with_sfs_context(opts, argc, argv, MOUNT_READONLY, do_export);
}

static int do_rm(sfs_runtime_context *context, command_options *opts, int argc, char *argv[]) {
    if (argc < 1) {
        return error("Missing required path argument for 'rm' command.");
    }
    const char *sfs_path = argv[0];
    bool is_dir = get_bool_option(opts, "is_dir");

    if (context->sfs_instance->unlink(context->sfs_instance, (char*)sfs_path, is_dir ? 1 : 0) != 0) {
        return error("Failed to remove '%s' (is_dir: %d).", sfs_path, is_dir);
    }

    printf("Successfully removed '%s' (is_dir: %d).\n", sfs_path, is_dir);
    return 0;
}

static int execute_rm(command_options *opts, int argc, char *argv[]) {
    return with_sfs_context(opts, argc, argv, MOUNT_READWRITE, do_rm);
}

static int execute_help(command_options *opts, int argc, char *argv[]) {
    if (argc == 0) {
        print_general_help();
        return 0;
    }

    const char *target_command_name = argv[0];
    const command_config *target_command = find_command(target_command_name, commands, NUM_COMMANDS);

    if (target_command == NULL) {
        return error("Unknown command for help '%s'", target_command_name);
    }

    // --- Print Usage Line ---
    printf("\nUsage: sfs %s", target_command->name);
    if (target_command->options) {
        printf(" [options]");
    }
    if (target_command->args) {
        for (int i = 0; target_command->args[i].name; i++) {
            printf(" %s", target_command->args[i].name);
        }
    }
    printf("\n");

    // --- Print Description ---
    printf("\nDescription: %s\n", target_command->description);

    // --- Print Options ---
    if (target_command->options) {
        printf("\nOptions:\n");
        for (int i = 0; target_command->options[i].long_name; i++) {
            const option_config *opt = &target_command->options[i];
            char opts_str[64] = "";
            if (opt->short_name)
                sprintf(opts_str, "-%c", opt->short_name);
            if (opt->long_name) {
                if (strlen(opts_str) > 0) strcat(opts_str, ", ");
                strcat(opts_str, opt->long_name);
            }
            // if (opt->has_argument) strcat(opts_str, "  <value>");
            printf("  %-25s %s\n",
                   opts_str,
                   opt->description);
        }
    }

    // --- Print Arguments ---
    if (target_command->args) {
        printf("\nArguments:\n");
        for (int i = 0; target_command->args[i].name; i++) {
            const arg_config *arg = &target_command->args[i];
            printf("  %-25s  %s%s\n",
                   arg->name,
                   arg->description,
                   arg->is_optional ? " (optional)" : "");
        }
    }

    printf("\n");
    return 0;
}

// --- General Help and Command Dispatching ---

void print_general_help() {
    printf("sfs - A tool for manipulating Simple File System images.\n\n");
    printf("Usage: sfs <image_file> <command> [options] [args...]\n");
    printf("       sfs help <command> (for more information on a specific command)\n\n");
    printf("Available commands:\n");
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (commands[i].name == NULL) continue;
        printf("  %-10s  %s\n", commands[i].name, commands[i].description);
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    return parse_and_dispatch_commands(argc, argv, commands, NUM_COMMANDS, print_general_help);
}
