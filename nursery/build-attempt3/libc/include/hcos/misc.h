#ifndef _HCOS_YIELD_H
#define _HCOS_YIELD_H


// HCOS extensions
int spawn(const char *path, char *const argv[], char *const envp[]);
void yield();



#endif // _HCOS_YIELD_H