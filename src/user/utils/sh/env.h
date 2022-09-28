#ifndef _ENV_H
#define _ENV_H

#include <ctypes.h>
#include <string.h>


extern char **envp;


void initenv();
char **getenvptr();

void setenv(char *varname, char *value);
char *getenv(char *varname);
void unsetenv(char *varname);



#endif
