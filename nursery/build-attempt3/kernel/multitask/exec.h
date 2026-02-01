#ifndef _EXEC_H
#define _EXEC_H

// exec a program in a given context
int execve(char *path, char *argv[], char *envp[]);

// exec a program without arguments or environment
int exec(char *path);



#endif
