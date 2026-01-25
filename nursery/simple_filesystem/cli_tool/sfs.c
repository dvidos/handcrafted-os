#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h> // For ftruncate
#include <errno.h>  // For errno and strerror

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
static int execute_mkfs(command_options *opts, int argc, char *argv[]);
static int execute_info(command_options *opts, int argc, char *argv[]);
static int execute_ls(command_options *opts, int argc, char *argv[]);
static int execute_mkdir(command_options *opts, int argc, char *argv[]);
static int execute_import(command_options *opts, int argc, char *argv[]);
static int execute_export(command_options *opts, int argc, char *argv[]);
static int execute_rm(command_options *opts, int argc, char *argv[]);
static int execute_help(command_options *opts, int argc, char *argv[]);


// --- Global Commands Array ---

// Forward declaration for print_general_help (used in execute_help and dispatcher)
void print_general_help(); 

const command_config commands[] = { // Not static so it can be passed to command_parser
    {"create", "Create a new disk image", execute_create, (const option_config[]){
        {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING},
        {"--size", 's', "Size of the volume (e.g., 20M)", true, "size", OPT_STRING},
        {NULL, 0, NULL, false, NULL, 0} // Terminator
    }},
    {"wrsect", "Write sectors from a file", execute_wrsect, (const option_config[]){
        {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING},
        {"--sector", 's', "Starting sector number to write to", true, "sector", OPT_INT},
        {"--file", 'f', "File to read data from", true, "file", OPT_STRING},
        {"--count", 'c', "Number of sectors to write (default: 1)", true, "count", OPT_INT},
        {NULL, 0, NULL, false, NULL, 0} // Terminator
    }},
    {"rdsect", "Read sectors to a file or stdout", execute_rdsect, (const option_config[]){
        {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING},
        {"--sector", 's', "Starting sector number to read from", true, "sector", OPT_INT},
        {"--file", 'f', "File to write data to (optional)", true, "file", OPT_STRING},
        {"--count", 'c', "Number of sectors to read (default: 1)", true, "count", OPT_INT},
        {NULL, 0, NULL, false, NULL, 0} // Terminator
    }},
    {"mkfs", "Create a filesystem on the disk image", execute_mkfs, (const option_config[]){
        {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING},
        {"--start-sector", 's', "Sector where the filesystem starts", true, "start", OPT_INT},
        {"--label", 'l', "Volume label", true, "label", OPT_STRING},
        {NULL, 0, NULL, false, NULL, 0} // Terminator
    }},
    {"info", "Display filesystem information", execute_info, (const option_config[]){
        {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING},
        {"--start-sector", 's', "Sector where the filesystem starts", true, "start", OPT_INT},
        {NULL, 0, NULL, false, NULL, 0} // Terminator
    }},
    {"ls", "List files in a directory", execute_ls, (const option_config[]){
        {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING},
        {"--start-sector", 's', "Sector where the filesystem starts", true, "start", OPT_INT},
        {NULL, 0, NULL, false, NULL, 0} // Terminator
    }},
    {"mkdir", "Create a directory", execute_mkdir, (const option_config[]){
        {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING},
        {"--start-sector", 's', "Sector where the filesystem starts", true, "start", OPT_INT},
        {NULL, 0, NULL, false, NULL, 0} // Terminator
    }},
    {"import", "Import a file from host to the filesystem", execute_import, (const option_config[]){
        {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING},
        {"--start-sector", 's', "Sector where the filesystem starts", true, "start", OPT_INT},
        {NULL, 0, NULL, false, NULL, 0} // Terminator
    }},
    {"export", "Export a file from the filesystem to host", execute_export, (const option_config[]){
        {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING},
        {"--start-sector", 's', "Sector where the filesystem starts", true, "start", OPT_INT},
        {NULL, 0, NULL, false, NULL, 0} // Terminator
    }},    {"rm", "Remove a file or directory", execute_rm, (const option_config[]){
        {"--image", 'i', "Path to the disk image file", true, "image", OPT_STRING},
        {"--start-sector", 's', "Sector where the filesystem starts", true, "start", OPT_INT},
        {"--dir", 'd', "Remove a directory (default: file)", false, "is_dir", OPT_BOOL},
        {NULL, 0, NULL, false, NULL, 0} // Terminator
    }},    {"help", "Display help for commands", execute_help, NULL},
    {NULL, NULL, NULL, NULL} // Terminator
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

// Function to initialize and set up the SFS context
static int setup_sfs_context(const char* image_file, long start_sector, int mount_readonly, sfs_runtime_context *context) {
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
    if (context->sfs_instance->mount(context->sfs_instance, mount_readonly) != 0) {
        return error("Failed to mount filesystem on '%s' starting at sector %ld.", image_file, start_sector);
    }
    return 0; // Success
}

static void cleanup_sfs_dependencies(sfs_runtime_context *context) {
    if (context->sfs_file_handle) {
        if (context->sfs_instance) {
            context->sfs_instance->close(context->sfs_instance, context->sfs_file_handle);
        }
    }
    if (context->sfs_instance) {
        context->sfs_instance->unmount(context->sfs_instance);
        // TODO: sfs_instance->release(sfs_instance); and its dependencies (clock_dev, mem_alloc, part_dev, base_dev)
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
    } else if (actual_bytes_read > total_bytes_to_write) {
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
        printf("--- Hexdump of %ld sectors starting from %ld ---", count, start_sector);
        hexdump_with_folding(buffer, total_bytes_to_read, start_sector * sector_size);
        printf("--- End of Hexdump ---");
    }

    free(buffer);    return 0;
}

static int execute_mkfs(command_options *opts, int argc, char *argv[]) {
    const char *image_file = get_str_option(opts, "image");
    if (image_file == NULL) return error("Missing required --image argument for 'mkfs' command.");
    long start_sector = get_int_option(opts, "start", -1);
    const char *label_str = get_str_option(opts, "label");
    sfs_runtime_context context; // Declare context struct

    if (start_sector == -1) {
        return error("Missing required --start-sector argument for 'mkfs' command.");
    }
    if (label_str == NULL) {
        return error("Missing required --label argument for 'mkfs' command.");
    }
    // Check label length
    if (strlen(label_str) > 31) { // Superblock has 32 chars for label including null terminator
        return error("Volume label must be 31 characters or less.");
    }

    // Setup SFS context
    if (setup_sfs_context(image_file, start_sector, 0 /* read-write */, &context) != 0) {
        cleanup_sfs_dependencies(&context); // Cleanup anything allocated within setup_sfs_context
        return -1; // setup_sfs_context already printed error
    }

    // Call mkfs
    uint32_t desired_block_size = 512; 
    if (context.sfs_instance->mkfs(context.sfs_instance, (char*)label_str, desired_block_size) != 0) {
        cleanup_sfs_dependencies(&context);
        return error("Failed to create filesystem on '%s'.", image_file);
    }

    printf("Successfully created SFS filesystem on '%s' starting at sector %ld with label '%s'.\n", image_file, start_sector, label_str);

    // Cleanup
    cleanup_sfs_dependencies(&context);

    return 0;
}
static int execute_info(command_options *opts, int argc, char *argv[]) {
    const char *image_file = get_str_option(opts, "image");
    if (image_file == NULL) return error("Missing required --image argument for 'info' command.");
    long start_sector = get_int_option(opts, "start", -1);
    sfs_runtime_context context;

    if (start_sector == -1) {
        return error("Missing required --start-sector argument for 'info' command.");
    }

    if (setup_sfs_context(image_file, start_sector, 1 /* readonly */, &context) != 0) {
        cleanup_sfs_dependencies(&context);
        return -1; // setup_sfs_context already printed error
    }

    printf("Filesystem Information for '%s' (partition at sector %ld):\n", image_file, start_sector);
    context.sfs_instance->dump_debug_info(context.sfs_instance, "Filesystem Info");

    // Cleanup
    cleanup_sfs_dependencies(&context);

    return 0;
}
static int execute_ls(command_options *opts, int argc, char *argv[]) {
    const char *image_file = get_str_option(opts, "image");
    if (image_file == NULL) return error("Missing required --image argument for 'ls' command.");
    long start_sector = get_int_option(opts, "start", -1);
    const char *path = "/"; // Default to root directory
    sfs_runtime_context context;

    if (argc > 0) { // Positional argument for path
        path = argv[0];
    }

    if (start_sector == -1) {
        return error("Missing required --start-sector argument for 'ls' command.");
    }

    if (setup_sfs_context(image_file, start_sector, 1 /* readonly */, &context) != 0) {
        cleanup_sfs_dependencies(&context);
        return -1; // setup_sfs_context already printed error
    }

    sfs_handle *dir_handle = NULL;
    if (context.sfs_instance->open_dir(context.sfs_instance, (char*)path, &dir_handle) != 0) {
        cleanup_sfs_dependencies(&context);
        return error("Could not open directory '%s'.", path);
    }
    // Assign dir_handle to context.sfs_file_handle for cleanup.
    context.sfs_file_handle = dir_handle;

    sfs_dir_entry entry;
    printf("Contents of '%s' (filesystem at sector %ld):\n", path, start_sector);
    while (context.sfs_instance->read_dir(context.sfs_instance, dir_handle, &entry) == 0) {
        if (entry.name[0] != '\0') { // Skip empty entries
            printf("  %s\n", entry.name);
        }
    }

    cleanup_sfs_dependencies(&context);

    return 0;
}

static int execute_mkdir(command_options *opts, int argc, char *argv[]) {
    const char *image_file = get_str_option(opts, "image");
    if (image_file == NULL) return error("Missing required --image argument for 'mkdir' command.");
    long start_sector = get_int_option(opts, "start", -1);
    const char *path = NULL;
    sfs_runtime_context context;

    if (argc > 0) { // Positional argument for path
        path = argv[0];
    }

    if (start_sector == -1) {
        return error("Missing required --start-sector argument for 'mkdir' command.");
    }
    if (path == NULL) {
        return error("Missing required path argument for 'mkdir' command.");
    }

    if (setup_sfs_context(image_file, start_sector, 0 /* read-write */, &context) != 0) {
        cleanup_sfs_dependencies(&context);
        return -1; // setup_sfs_context already printed error
    }

    if (context.sfs_instance->create(context.sfs_instance, (char*)path, 1 /* is_dir */) != 0) {
        cleanup_sfs_dependencies(&context);
        return error("Failed to create directory '%s'.", path);
    }

    printf("Successfully created directory '%s' on filesystem at sector %ld.\n", path, start_sector);

    cleanup_sfs_dependencies(&context);

    return 0;
}


static int execute_import(command_options *opts, int argc, char *argv[]) {
    const char *image_file = get_str_option(opts, "image");
    if (image_file == NULL) return error("Missing required --image argument for 'import' command.");
    long start_sector = get_int_option(opts, "start", -1);
    const char *source_host_file = NULL;
    const char *destination_sfs_path = NULL;
    sfs_runtime_context context;

    if (start_sector == -1) {
        return error("Missing required --start-sector argument for 'import' command.");
    }

    if (argc < 2) {
        return error("Missing required arguments. Usage: import --start-sector <sector> <source_host_file> <destination_sfs_path>");
    }

    source_host_file = argv[0];
    destination_sfs_path = argv[1];

    if (source_host_file == NULL || destination_sfs_path == NULL) {
        return error("Missing required source_host_file or destination_sfs_path argument for 'import' command.");
    }

    // Setup SFS context
    if (setup_sfs_context(image_file, start_sector, 0 /* read-write */, &context) != 0) {
        cleanup_sfs_dependencies(&context);
        return -1; // setup_sfs_context already printed error
    }

    context.host_fp = fopen(source_host_file, "rb");
    if (!context.host_fp) {
        cleanup_sfs_dependencies(&context);
        return error("Error opening host file '%s': %s", source_host_file, strerror(errno));
    }

    // --- Create SFS file ---
    if (context.sfs_instance->create(context.sfs_instance, (char*)destination_sfs_path, 0 /* not a directory */) != 0) {
        fprintf(stderr, "Warning: Failed to create file '%s' in SFS. Attempting to open for overwrite. If this is not intended, ensure the file does not exist.\n", destination_sfs_path);
    }

    // --- Open SFS file for writing ---
    if (context.sfs_instance->open(context.sfs_instance, (char*)destination_sfs_path, SFS_O_WRONLY, &context.sfs_file_handle) != 0) {
        cleanup_sfs_dependencies(&context);
        return error("Failed to open SFS file '%s' for writing.", destination_sfs_path);
    }
    if (context.sfs_file_handle == NULL) {
        cleanup_sfs_dependencies(&context);
        return error("SFS file handle is NULL after opening '%s'.", destination_sfs_path);
    }

    // --- Read from host file and write to SFS file ---
    uint32_t sector_size = context.base_dev->get_sector_size(context.base_dev);
    context.buffer = malloc(sector_size);
    if (!context.buffer) {
        cleanup_sfs_dependencies(&context);
        return error("Failed to allocate buffer for import operation.");
    }

    size_t bytes_read;
    int total_bytes_imported = 0;
    while ((bytes_read = fread(context.buffer, 1, sector_size, context.host_fp)) > 0) {
        int written_bytes = context.sfs_instance->write(context.sfs_instance, context.sfs_file_handle, context.buffer, bytes_read);
        if (written_bytes < 0 || (size_t)written_bytes != bytes_read) {
            cleanup_sfs_dependencies(&context);
            return error("Error writing to SFS file '%s'. Wrote %d bytes, expected %zu.", destination_sfs_path, written_bytes, bytes_read);
        }
        total_bytes_imported += written_bytes;
    }

    if (ferror(context.host_fp)) {
        cleanup_sfs_dependencies(&context);
        return error("Error reading from host file '%s'.", source_host_file);
    }

    // --- Cleanup ---
    cleanup_sfs_dependencies(&context);

    printf("Successfully imported '%s' (host) to '%s' (SFS) with %d bytes.\n", source_host_file, destination_sfs_path, total_bytes_imported);

    return 0;
}
static int execute_export(command_options *opts, int argc, char *argv[]) {
    const char *image_file = get_str_option(opts, "image");
    if (image_file == NULL) return error("Missing required --image argument for 'export' command.");
    long start_sector = get_int_option(opts, "start", -1);
    const char *source_sfs_path = NULL;
    const char *destination_host_file = NULL;
    sfs_runtime_context context;

    if (start_sector == -1) {
        return error("Missing required --start-sector argument for 'export' command.");
    }

    if (argc < 2) {
        return error("Missing required arguments. Usage: export --start-sector <sector> <source_sfs_path> <destination_host_file>");
    }

    source_sfs_path = argv[0];
    destination_host_file = argv[1];

    if (source_sfs_path == NULL || destination_host_file == NULL) {
        return error("Missing required source_sfs_path or destination_host_file argument for 'export' command.");
    }

    // Setup SFS context
    if (setup_sfs_context(image_file, start_sector, 1 /* readonly */, &context) != 0) {
        cleanup_sfs_dependencies(&context);
        return -1; // setup_sfs_context already printed error
    }

    // --- Open SFS file for reading ---
    if (context.sfs_instance->open(context.sfs_instance, (char*)source_sfs_path, SFS_O_RDONLY, &context.sfs_file_handle) != 0) {
        cleanup_sfs_dependencies(&context);
        return error("Failed to open SFS file '%s' for reading.", source_sfs_path);
    }
    if (context.sfs_file_handle == NULL) {
        cleanup_sfs_dependencies(&context);
        return error("SFS file handle is NULL after opening '%s'.", source_sfs_path);
    }

    // --- Open host file for writing ---
    context.host_fp = fopen(destination_host_file, "wb");
    if (!context.host_fp) {
        cleanup_sfs_dependencies(&context);
        return error("Error opening host file '%s' for writing: %s", destination_host_file, strerror(errno));
    }

    // --- Read from SFS file and write to host file ---
    uint32_t sector_size = context.base_dev->get_sector_size(context.base_dev); // Use base_dev to get sector size
    context.buffer = malloc(sector_size);
    if (!context.buffer) {
        cleanup_sfs_dependencies(&context);
        return error("Failed to allocate buffer for export operation.");
    }

    size_t bytes_read;
    int total_bytes_exported = 0;
    while ((bytes_read = context.sfs_instance->read(context.sfs_instance, context.sfs_file_handle, context.buffer, sector_size)) > 0) {
        size_t written_bytes = fwrite(context.buffer, 1, bytes_read, context.host_fp);
        if (written_bytes != bytes_read) {
            cleanup_sfs_dependencies(&context);
            return error("Error writing to host file '%s'. Wrote %zu bytes, expected %zu.", destination_host_file, written_bytes, bytes_read);
        }
        total_bytes_exported += bytes_read;
    }
    
    // Check for read errors from SFS (sfs_instance->read returns -1 on error)
    if ((int)bytes_read == -1) {
        cleanup_sfs_dependencies(&context);
        return error("Error reading from SFS file '%s'.", source_sfs_path);
    }

    // --- Cleanup ---
    cleanup_sfs_dependencies(&context);

    printf("Successfully exported '%s' (SFS) to '%s' (host) with %d bytes.\n", source_sfs_path, destination_host_file, total_bytes_exported);

    return 0;
}
static int execute_rm(command_options *opts, int argc, char *argv[]) {
    const char *image_file = get_str_option(opts, "image");
    if (image_file == NULL) return error("Missing required --image argument for 'rm' command.");
    long start_sector = get_int_option(opts, "start", -1);
    bool is_dir = get_bool_option(opts, "is_dir");
    const char *sfs_path = NULL;
    sfs_runtime_context context; // Declare context struct

    if (start_sector == -1) {
        return error("Missing required --start-sector argument for 'rm' command.");
    }

    if (argc < 1) {
        return error("Missing required argument. Usage: rm --start-sector <sector> [--dir] <sfs_path>");
    }

    sfs_path = argv[0];

    // Setup SFS context
    if (setup_sfs_context(image_file, start_sector, 0 /* read-write */, &context) != 0) {
        cleanup_sfs_dependencies(&context);
        return -1; // setup_sfs_context already printed error
    }

    // --- Unlink file or directory ---
    int unlink_option = is_dir ? 1 : 0; // 1 for directory, 0 for file
    if (context.sfs_instance->unlink(context.sfs_instance, (char*)sfs_path, unlink_option) != 0) {
        cleanup_sfs_dependencies(&context);
        return error("Failed to remove '%s' (is_dir: %d).", sfs_path, is_dir);
    }

    // --- Cleanup ---
    cleanup_sfs_dependencies(&context);

    printf("Successfully removed '%s' (is_dir: %d) from filesystem at sector %ld.\n", sfs_path, is_dir, start_sector);

    return 0;
}
static int execute_help(command_options *opts, int argc, char *argv[]) {
    if (argc == 0) {
        print_general_help();
        return 0;
    }

    const char *target_command_name = argv[0];
    const command_config *target_command = find_command(target_command_name, commands, NUM_COMMANDS); // Pass commands and NUM_COMMANDS

    if (target_command == NULL) {
        return error("Unknown command for help '%s'", target_command_name);
    }

    printf("\nCommand: %s\n", target_command->name);
    printf("Description: %s\n", target_command->description);    if (target_command->options) {
        printf("\nOptions:\n");
        for (int i = 0; target_command->options[i].long_name; i++) {
            const option_config *opt = &target_command->options[i];
            char short_opt_str[5] = ""; // \"-s\0\" or \"   \0\"
            if (opt->short_name) {
                sprintf(short_opt_str, ", -%c", opt->short_name);
            }
            const char *arg_description = opt->has_argument ? " <value>" : "";
            printf("  %s%s%s  %s\n",
                   opt->long_name,
                   short_opt_str,
                   arg_description,
                   opt->description);
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
