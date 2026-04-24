#include "../libc_internal.h"


// Define the global FILE objects
FILE __stdin = {
    .fd = STDIN_FILENO,
    .buffer = NULL,
    .buf_size = 0,
    .pos = 0,
    .end = 0,
    .flags = 0,
    .ungetc_char = 0,
    .has_ungetc_char = false
};

FILE __stdout = {
    .fd = STDOUT_FILENO,
    .buffer = NULL,
    .buf_size = 0,
    .pos = 0,
    .end = 0,
    .flags = 0,
    .ungetc_char = 0,
    .has_ungetc_char = false
};

FILE __stderr = {
    .fd = STDERR_FILENO,
    .buffer = NULL,
    .buf_size = 0,
    .pos = 0,
    .end = 0,
    .flags = 0,
    .ungetc_char = 0,
    .has_ungetc_char = false
};

// Pointers to the global FILE objects (as declared in <stdio.h>)
FILE *stdin = &__stdin;
FILE *stdout = &__stdout;
FILE *stderr = &__stderr;
FILE *__open_files_list = NULL;


// Function to initialize stdin, stdout, stderr
void __init_stdio(void) {
    // For stdin, use full buffering by default, with read mode
    stdin = fdopen(STDIN_FILENO, "r");
    if (stdin) {
        setvbuf(stdin, NULL, _IOLBF, 0); // Line buffering
    } else {
        // Handle error: possibly set a global flag or fallback to unbuffered
        // For now, if fdopen fails, we'll proceed with potentially invalid stdin
        // or rely on default values which might be incorrect.
        // A more robust system might panic or log an error.
    }

    // For stdout, use full buffering by default, with write mode
    stdout = fdopen(STDOUT_FILENO, "w");
    if (stdout) {
        setvbuf(stdout, NULL, _IOLBF, 0); // Line buffering
    } else {
        // Handle error
    }

    // For stderr, usually unbuffered or line-buffered, and write mode
    // Using unbuffered for stderr to ensure immediate error output
    stderr = fdopen(STDERR_FILENO, "w");
    if (stderr) {
        setvbuf(stderr, NULL, _IONBF, 0); // Set stderr to unbuffered
    } else {
        // Handle error
    }
}
