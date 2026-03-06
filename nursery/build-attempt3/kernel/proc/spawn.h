#ifndef _SPAWN_H
#define _SPAWN_H

// exec a program in a given context
int spawnve(char *path, char *argv[], char *envp[]);

// exec a program without arguments or environment
int spawn(char *path);



#endif
