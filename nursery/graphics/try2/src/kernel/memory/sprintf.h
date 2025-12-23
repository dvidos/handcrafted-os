#include <stdarg.h>


void sprintfn(char *buffer, int buffsize, const char *format, ...);
void vsprintfn(char *buffer, int buffsize, const char *format, va_list args);
