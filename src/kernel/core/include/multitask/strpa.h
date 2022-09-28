#ifndef _STRPA_H
#define _STRPA_H

// work with constructs like argv and envp
// "arrays" are essentially a series of pointers with the last pointer being NULL
char **create_str_ptr_arr();
int count_str_ptr_arr(char **ptr);
char **duplicate_str_ptr_arr(char **ptr);
void free_str_ptr_arr(char **ptr);
void debug_str_ptr_arr(char *varname, char **ptr);


#endif
