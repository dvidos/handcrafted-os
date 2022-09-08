#ifndef _PROC_H
#define _PROC_H

int getpid();
int getppid();

int fork();
int wait(int *exit_status);

int exec(char *path, char **argv, char **envp);

#endif
