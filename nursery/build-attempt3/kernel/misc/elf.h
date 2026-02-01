#ifndef _ELF_H
#define _ELF_H


// i want to keep the exec() and elf() concerns separate
// but loading maybe more complicated than that....
// I think linux and unices have all inclusive loaders, that both load and execute...

// but we definitely want to be able to also run kernel tasks, not just executables.
// though, for those we wouldn't call exec()...


// returns SUCCESS or ERR_NOT_SUPPORTED accordingly
int verify_elf_executable(file_t *file);

// calcualtes information for setting up a new process
int get_elf_load_information(file_t *file, void **virt_addr_start, void **virt_addr_end, void **entry_point);

// loads segments from the file into memory
int load_elf_into_memory(file_t *file);

// logs debug information about the elf, and how we understand it.
int dump_elf_information(file_t *file);






#endif 