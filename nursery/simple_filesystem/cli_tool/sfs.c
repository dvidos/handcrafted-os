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

static int execute_create(const char* image_file, command_options *opts, int argc, char *argv[]);
static int execute_wrsect(const char* image_file, command_options *opts, int argc, char *argv[]);
static int execute_rdsect(const char* image_file, command_options *opts, int argc, char *argv[]);
static int execute_mkfs(const char* image_file, command_options *opts, int argc, char *argv[]);
static int execute_info(const char* image_file, command_options *opts, int argc, char *argv[]);
static int execute_ls(const char* image_file, command_options *opts, int argc, char *argv[]);
static int execute_mkdir(const char* image_file, command_options *opts, int argc, char *argv[]);
static int execute_import(const char* image_file, command_options *opts, int argc, char *argv[]);
static int execute_export(const char* image_file, command_options *opts, int argc, char *argv[]);
static int execute_rm(const char* image_file, command_options *opts, int argc, char *argv[]);
static int execute_help(const char* image_file, command_options *opts, int argc, char *argv[]);


// --- Global Commands Array ---

// Forward declaration for print_general_help (used in execute_help and dispatcher)
void print_general_help(); 

const command_config commands[] = { // Not static so it can be passed to command_parser
    {"create", "Create a new disk image", execute_create, (const option_config[]){
        {"--size", 's', "Size of the volume (e.g., 20M)", true, "size", OPT_STRING},
        {NULL, 0, NULL, false, NULL, 0} // Terminator
    }},
    {"wrsect", "Write sectors from a file", execute_wrsect, (const option_config[]){
        {"--sector", 's', "Starting sector number to write to", true, "sector", OPT_INT},
        {"--file", 'f', "File to read data from", true, "file", OPT_STRING},
        {"--count", 'c', "Number of sectors to write (default: 1)", true, "count", OPT_INT},
        {NULL, 0, NULL, false, NULL, 0} // Terminator
    }},
    {"rdsect", "Read sectors to a file or stdout", execute_rdsect, (const option_config[]){
        {"--sector", 's', "Starting sector number to read from", true, "sector", OPT_INT},
        {"--file", 'f', "File to write data to (optional)", true, "file", OPT_STRING},
        {"--count", 'c', "Number of sectors to read (default: 1)", true, "count", OPT_INT},
        {NULL, 0, NULL, false, NULL, 0} // Terminator
    }},
    {"mkfs", "Create a filesystem on the disk image", execute_mkfs, (const option_config[]){
        {"--start-sector", 's', "Sector where the filesystem starts", true, "start", OPT_INT},
        {"--label", 'l', "Volume label", true, "label", OPT_STRING},
        {NULL, 0, NULL, false, NULL, 0} // Terminator
    }},
    {"info", "Display filesystem information", execute_info, (const option_config[]){
        {"--start-sector", 's', "Sector where the filesystem starts", true, "start", OPT_INT},
        {NULL, 0, NULL, false, NULL, 0} // Terminator
    }},
    {"ls", "List files in a directory", execute_ls, (const option_config[]){
        {"--start-sector", 's', "Sector where the filesystem starts", true, "start", OPT_INT},
        {NULL, 0, NULL, false, NULL, 0} // Terminator
    }},
    {"mkdir", "Create a directory", execute_mkdir, (const option_config[]){
        {"--start-sector", 's', "Sector where the filesystem starts", true, "start", OPT_INT},
        {NULL, 0, NULL, false, NULL, 0} // Terminator
    }},
    {"import", "Import a file from host to the filesystem", execute_import, (const option_config[]){
        {"--start-sector", 's', "Sector where the filesystem starts", true, "start", OPT_INT},
        {NULL, 0, NULL, false, NULL, 0} // Terminator
    }},
    {"export", "Export a file from the filesystem to host", execute_export, NULL},    {"rm", "Remove a file or directory", execute_rm, NULL},    {"help", "Display help for commands", execute_help, NULL},
    {NULL, NULL, NULL, NULL} // Terminator
};
const size_t NUM_COMMANDS = (sizeof(commands) / sizeof(commands[0]) -1); // -1 because of NULL terminator


// --- Command Implementations ---

static int execute_create(const char* image_file, command_options *opts, int argc, char *argv[]) {
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

static int execute_wrsect(const char* image_file, command_options *opts, int argc, char *argv[]) {
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

static int execute_rdsect(const char* image_file, command_options *opts, int argc, char *argv[]) {
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

static int execute_mkfs(const char* image_file, command_options *opts, int argc, char *argv[]) {
    long start_sector = get_int_option(opts, "start", -1);
    const char *label_str = get_str_option(opts, "label");

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

    // 1. Create file_sector_device
    sector_device *base_dev = new_file_sector_device(image_file);
    if (!base_dev) {
        return error("Could not open image file '%s'.", image_file);
    }

    uint32_t total_image_sectors = base_dev->get_sector_count(base_dev);
    if (start_sector >= total_image_sectors) return error("Start sector %ld is beyond total image sectors %u.", start_sector, total_image_sectors);
    uint32_t partition_sector_count = total_image_sectors - start_sector;
    if (partition_sector_count == 0) return error("Partition has zero sectors.");


    // 2. Create partition_sector_device
    sector_device *part_dev = new_partition_sector_device(base_dev, start_sector, partition_sector_count);
    if (!part_dev) return error("Could not create partition device.");

    // 3. Create mem_allocator and clock_device
    mem_allocator *mem_alloc = new_malloc_based_mem_allocator();
    if (!mem_alloc) return error("Could not create memory allocator.");
    clock_device *clock_dev = new_rtc_based_clock_device();
    if (!clock_dev) return error("Could not create clock device.");

    // 4. Create simple_filesystem instance
    simple_filesystem *sfs_instance = new_simple_filesystem(mem_alloc, part_dev, clock_dev);
    if (!sfs_instance) return error("Could not create simple_filesystem instance.");

    // 5. Call mkfs
    // For now, hardcode desired_block_size to 512 bytes
    uint32_t desired_block_size = 512; 
    if (sfs_instance->mkfs(sfs_instance, (char*)label_str, desired_block_size) != 0) 
        return error("Failed to create filesystem on '%s'.", image_file);

    printf("Successfully created SFS filesystem on '%s' starting at sector %ld with label '%s'.\n", image_file, start_sector, label_str);

    // 6. Cleanup (TODO: implement proper free functions in simple_filesystem and its dependencies)
    // sfs_instance->release(sfs_instance); // Assuming a release function
    // clock_dev->release(clock_dev);
    // mem_alloc->release(mem_alloc);
    // part_dev->release(part_dev);
    // base_dev->release(base_dev);

    return 0;
}
static int execute_info(const char* image_file, command_options *opts, int argc, char *argv[]) {
    long start_sector = get_int_option(opts, "start", -1);

    if (start_sector == -1) {
        return error("Missing required --start-sector argument for 'info' command.");
    }

    // 1. Create file_sector_device
    sector_device *base_dev = new_file_sector_device(image_file);
    if (!base_dev) {
        return error("Could not open image file '%s'.", image_file);
    }

    uint32_t total_image_sectors = base_dev->get_sector_count(base_dev);
    if (start_sector >= total_image_sectors) return error("Start sector %ld is beyond total image sectors %u.", start_sector, total_image_sectors);
    uint32_t partition_sector_count = total_image_sectors - start_sector;
    if (partition_sector_count == 0) return error("Partition has zero sectors.");

    // 2. Create partition_sector_device
    sector_device *part_dev = new_partition_sector_device(base_dev, start_sector, partition_sector_count);
    if (!part_dev) return error("Could not create partition device.");

    // 3. Create mem_allocator and clock_device
    mem_allocator *mem_alloc = new_malloc_based_mem_allocator();
    if (!mem_alloc) return error("Could not create memory allocator.");
    clock_device *clock_dev = new_rtc_based_clock_device();
    if (!clock_dev) return error("Could not create clock device.");

    // 4. Create simple_filesystem instance
    simple_filesystem *sfs_instance = new_simple_filesystem(mem_alloc, part_dev, clock_dev);
    if (!sfs_instance) return error("Could not create simple_filesystem instance.");

    // 5. Mount the filesystem
    if (sfs_instance->mount(sfs_instance, 1 /* readonly */) != 0) 
        return error("Failed to mount filesystem on '%s' starting at sector %ld.", image_file, start_sector);

    printf("Filesystem Information for '%s' (partition at sector %ld):\n", image_file, start_sector);
    sfs_instance->dump_debug_info(sfs_instance, "Filesystem Info");

    // 6. Cleanup (TODO: implement proper free functions in simple_filesystem and its dependencies)
    // sfs_instance->unmount(sfs_instance); // Assuming unmount is required before release
    // clock_dev->release(clock_dev);
    // mem_alloc->release(mem_alloc);
    // part_dev->release(part_dev);
    // base_dev->release(base_dev);

    return 0;
}
static int execute_ls(const char* image_file, command_options *opts, int argc, char *argv[]) {
    long start_sector = get_int_option(opts, "start", -1);
    const char *path = "/"; // Default to root directory

    if (argc > 0) { // Positional argument for path
        path = argv[0];
    }

    if (start_sector == -1) {
        return error("Missing required --start-sector argument for 'ls' command.");
    }

    // 1. Create file_sector_device
    sector_device *base_dev = new_file_sector_device(image_file);
    if (!base_dev) {
        return error("Could not open image file '%s'.", image_file);
    }

    uint32_t total_image_sectors = base_dev->get_sector_count(base_dev);
    if (start_sector >= total_image_sectors) return error("Start sector %ld is beyond total image sectors %u.", start_sector, total_image_sectors);
    uint32_t partition_sector_count = total_image_sectors - start_sector;
    if (partition_sector_count == 0) return error("Partition has zero sectors.");

    // 2. Create partition_sector_device
    sector_device *part_dev = new_partition_sector_device(base_dev, start_sector, partition_sector_count);
    if (!part_dev) return error("Could not create partition device.");

    // 3. Create mem_allocator and clock_device
    mem_allocator *mem_alloc = new_malloc_based_mem_allocator();
    if (!mem_alloc) return error("Could not create memory allocator.");
    clock_device *clock_dev = new_rtc_based_clock_device();
    if (!clock_dev) return error("Could not create clock device.");

    // 4. Create simple_filesystem instance
    simple_filesystem *sfs_instance = new_simple_filesystem(mem_alloc, part_dev, clock_dev);
    if (!sfs_instance) return error("Could not create simple_filesystem instance.");

    // 5. Mount the filesystem
    if (sfs_instance->mount(sfs_instance, 1 /* readonly */) != 0) 
        return error("Failed to mount filesystem on '%s' starting at sector %ld.", image_file, start_sector);

    sfs_handle *dir_handle = NULL;
    if (sfs_instance->open_dir(sfs_instance, (char*)path, &dir_handle) != 0) return error("Could not open directory '%s'.", path);

    sfs_dir_entry entry;
    printf("Contents of '%s' (filesystem at sector %ld):\n", path, start_sector);
    while (sfs_instance->read_dir(sfs_instance, dir_handle, &entry) == 0) {
        if (entry.name[0] != '\0') { // Skip empty entries
            printf("  %s\n", entry.name);
        }
    }

    sfs_instance->close_dir(sfs_instance, dir_handle);
    sfs_instance->unmount(sfs_instance); // Unmount before cleanup

    // 6. Cleanup (TODO: implement proper free functions)
    // sfs_instance->release(sfs_instance);
    // clock_dev->release(clock_dev);
    // mem_alloc->release(mem_alloc);
    // part_dev->release(part_dev);
    // base_dev->release(base_dev);

    return 0;
}
static int execute_mkdir(const char* image_file, command_options *opts, int argc, char *argv[]) {
    long start_sector = get_int_option(opts, "start", -1);
    const char *path = NULL;

    if (argc > 0) { // Positional argument for path
        path = argv[0];
    }

    if (start_sector == -1) {
        return error("Missing required --start-sector argument for 'mkdir' command.");
    }
    if (path == NULL) {
        return error("Missing required path argument for 'mkdir' command.");
    }

    // 1. Create file_sector_device
    sector_device *base_dev = new_file_sector_device(image_file);
    if (!base_dev) {
        return error("Could not open image file '%s'.", image_file);
    }

    uint32_t total_image_sectors = base_dev->get_sector_count(base_dev);
    if (start_sector >= total_image_sectors) return error("Start sector %ld is beyond total image sectors %u.", start_sector, total_image_sectors);
    uint32_t partition_sector_count = total_image_sectors - start_sector;
    if (partition_sector_count == 0) return error("Partition has zero sectors.");

    // 2. Create partition_sector_device
    sector_device *part_dev = new_partition_sector_device(base_dev, start_sector, partition_sector_count);
    if (!part_dev) return error("Could not create partition device.");

    // 3. Create mem_allocator and clock_device
    mem_allocator *mem_alloc = new_malloc_based_mem_allocator();
    if (!mem_alloc) return error("Could not create memory allocator.");
    clock_device *clock_dev = new_rtc_based_clock_device();
    if (!clock_dev) return error("Could not create clock device.");

    // 4. Create simple_filesystem instance
    simple_filesystem *sfs_instance = new_simple_filesystem(mem_alloc, part_dev, clock_dev);
    if (!sfs_instance) return error("Could not create simple_filesystem instance.");

    // 5. Mount the filesystem
    if (sfs_instance->mount(sfs_instance, 0 /* read-write */) != 0) 
        return error("Failed to mount filesystem on '%s' starting at sector %ld.", image_file, start_sector);

    if (sfs_instance->create(sfs_instance, (char*)path, 1 /* is_dir */) != 0) 
        return error("Failed to create directory '%s'.", path);

    printf("Successfully created directory '%s' on filesystem at sector %ld.\n", path, start_sector);

    sfs_instance->unmount(sfs_instance); // Unmount before cleanup

    // 6. Cleanup (TODO: implement proper free functions)
    // sfs_instance->release(sfs_instance);
    // clock_dev->release(clock_dev);
    // mem_alloc->release(mem_alloc);
    // part_dev->release(part_dev);
    // base_dev->release(base_dev);

    return 0;
}
static int execute_import(const char* image_file, command_options *opts, int argc, char *argv[]) { printf("import not implemented\n"); return 1; }
static int execute_export(const char* image_file, command_options *opts, int argc, char *argv[]) { printf("export not implemented\n"); return 1; }
static int execute_rm(const char* image_file, command_options *opts, int argc, char *argv[]) { printf("rm not implemented\n"); return 1; }

static int execute_help(const char* image_file, command_options *opts, int argc, char *argv[]) {
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
