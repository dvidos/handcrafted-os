#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h> // For NULL, size_t
#include <sys/types.h> // For off_t
#include <stdarg.h> // For va_list

// --- Type Definitions ---
// Placeholder for FILE structure. A real implementation would define its internal members.
typedef struct _IO_FILE FILE;

// --- Standard Streams (externally defined) ---
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

// --- Constants ---
#define EOF (-1)
#define BUFSIZ 1024 // Or some other reasonable buffer size
#define FOPEN_MAX 16 // Max number of open files
#define FILENAME_MAX 256 // Max length of a filename

// seek whence values
#define SEEK_SET 0 // Seek from beginning of file
#define SEEK_CUR 1 // Seek from current file position
#define SEEK_END 2 // Seek from end of file

// --- Function Prototypes (based on usage analysis) ---

// Output functions
int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vprintf(const char *format, va_list ap);
int vfprintf(FILE *stream, const char *format, va_list ap);
int vsprintf(char *str, const char *format, va_list ap);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
int puts(const char *s);
int fputs(const char *s, FILE *stream);
int putchar(int c);
int fputc(int c, FILE *stream);

// Input functions
int getchar(void);
int fgetc(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);

// File access and operations
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
FILE *fopen(const char *filename, const char *mode);
int fclose(FILE *stream);
int fflush(FILE *stream);
int fseek(FILE *stream, off_t offset, int whence);
long ftell(FILE *stream);
void rewind(FILE *stream);

// Error-handling functions
int feof(FILE *stream);
int ferror(FILE *stream);
void clearerr(FILE *stream);
void perror(const char *s);

// File manipulation
int remove(const char *filename);
int rename(const char *oldname, const char *newname);

#endif // _STDIO_H