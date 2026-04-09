#include "../libc_internal.h"

// Defined in stdio_init.c
extern FILE *__open_files_list;

void __flush_all_files(void) {
    FILE *current = __open_files_list;
    while (current != NULL) {
        // Only flush if the file is open for writing or appending
        // and has _IO_FULL_BUF or _IO_LINE_BUF set.
        // Unbuffered streams don't need explicit flushing.
        if ((current->flags & _IO_WRITE || current->flags & _IO_APPEND) &&
            (current->flags & _IO_FULL_BUF || current->flags & _IO_LINE_BUF)) {
            fflush(current);
        }
        current = current->next;
    }
}