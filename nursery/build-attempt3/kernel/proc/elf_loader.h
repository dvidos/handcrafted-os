#pragma once
#include "../filesys/vfs_api.h"


// i want to keep the exec() and elf() concerns separate
// but loading maybe more complicated than that....
// I think linux and unices have all inclusive loaders, that both load and execute...

// but we definitely want to be able to also run kernel tasks, not just executables.
// though, for those we wouldn't call exec()...


// returns OK or ERR_NOT_SUPPORTED accordingly
int verify_elf_executable(open_file_t *file);

// calcualtes information for setting up a new process
int get_elf_load_information(open_file_t *file, virt_addr_t *virt_addr_start, virt_addr_t *virt_addr_end, virt_addr_t *entry_point);

// loads segments from the file into memory
int load_elf_into_memory(open_file_t *file);

// logs debug information about the elf, and how we understand it.
int dump_elf_information(open_file_t *file);





