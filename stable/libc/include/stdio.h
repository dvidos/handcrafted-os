#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h> // For NULL, size_t
#include <sys/types.h> // For off_t
#include <stdarg.h> // For va_list

// --- Type Definitions ---
// Placeholder for FILE structure. A real implementation would define its internal members.
typedef struct _IO_FILE FILE;

// File position type (opaque)
typedef long fpos_t;

// --- Standard Streams (externally defined) ---
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

// --- Constants ---
#define EOF (-1)
#define BUFSIZ 1024 // Or some other reasonable buffer size
#define FOPEN_MAX 16 // Max number of open files
#define FILENAME_MAX 256 // Max length of a filename

// Buffer modes for setvbuf
#define _IOFBF 0    // Fully buffered
#define _IOLBF 1    // Line buffered
#define _IONBF 2    // Unbuffered

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
int getc(FILE *stream); // Also common, often a macro for fgetc
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
char *fgets(char *s, int size, FILE *stream);
int ungetc(int c, FILE *stream);

// File access and operations
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
FILE *fopen(const char *filename, const char *mode);
FILE *fdopen(int fd, const char *mode); // Associate a stream with an existing file descriptor
int fclose(FILE *stream);
int fflush(FILE *stream);
int fseek(FILE *stream, off_t offset, int whence);
long ftell(FILE *stream);
int fgetpos(FILE *stream, fpos_t *pos);
int fsetpos(FILE *stream, const fpos_t *pos);
void rewind(FILE *stream);

// Buffer control
void setbuf(FILE *stream, char *buf);
int setvbuf(FILE *stream, char *buf, int mode, size_t size);

// Error-handling functions
int feof(FILE *stream);
int ferror(FILE *stream);
void clearerr(FILE *stream);
void perror(const char *s);

// File manipulation
int remove(const char *filename);
int rename(const char *oldname, const char *newname);
int mkstemp(char *template); // Create unique temporary file

// Get file descriptor
int fileno(FILE *stream);

#endif // _STDIO_H