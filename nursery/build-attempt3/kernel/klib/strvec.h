#ifndef _STRVEC_H
#define _STRVEC_H

// work with constructs like argv and envp
// called string vectors here for convenience
// they are arrays of str pointers, with the last element being a NULL pointer

int count_strvec(char **strvec);

char **clone_strvec(char **strvec);

void free_strvec(char **strvec);

void debug_strvec(char *varname, char **strvec);


#endif
