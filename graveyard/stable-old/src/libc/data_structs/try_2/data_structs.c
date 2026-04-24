#include "data_structs.h"


malloc_function *data_structs_malloc;
free_function *data_structs_free;
printf_function *data_structs_warn;


void init_data_structs(malloc_function *malloc_function, free_function *free_function, printf_function *warn) {
    data_structs_malloc = malloc_function;
    data_structs_free = free_function;
    data_structs_warn = warn;
}
