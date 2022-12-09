#ifndef _PRINTF_H
#define _PRINTF_H

#include "bios.h"
#include "funcs.h"


void vsprintfn(char *buffer, int buffsize, const char *format, va_list args);
void sprintfn(char *buffer, int buffsize, const char *format, ...);
void printf(const char *format, ...);



#endif
