#ifndef _FUNCS_H
#define _FUNCS_H

#include "types.h"


void itoa(word num, char *buffer, int base);
void itosize(uint32 value, char *buffer);

uint32 strlen(char *str);
void   strcpy(char *target, const char *source);
void   strcat(char *target, char *source);

void memset(char *target, char c, uint32 size);
void memcpy(char *dest, char *source, uint32 size);
int  memcmp(char *a, char *b, uint32 size);


#endif
