#pragma once
#include "../filesys/vfs_api.h"



// returns OK or ERR_NOT_SUPPORTED accordingly
error_t elf_verify_executable(open_file_t *file);

// calculates information for setting up a new process
error_t elf_get_loading_information(open_file_t *file, virt_addr_t *virt_addr_start, virt_addr_t *virt_addr_end, virt_addr_t *entry_point);

// loads segments from the file into memory
error_t elf_load_into_memory(open_file_t *file);

// logs debug information about the elf, and how we understand it.
error_t elf_dump_information(open_file_t *file);


typedef struct loadable_segment {
    size_t offset_in_file;
    size_t size_in_file;
    virt_addr_t address_in_mem;
    size_t size_in_mem;
    bool writable;
    bool executable;
} elf_loadable_segment_t;

error_t elf_get_entry_point(open_file_t *file, virt_addr_t *entry_point);
error_t elf_get_program_headers_count(open_file_t *file, int *count);
error_t elf_get_program_headers_info(open_file_t *file, elf_loadable_segment_t *segments_arr, int count);

void elf_segment_formatter(log_write_stream_t *stream, va_list args);