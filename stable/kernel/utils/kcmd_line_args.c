#include "kcmd_line_args.h"
#include "../klib/string.h"


#define KCMD_MAX_ARGS 24

static kcmd_arg_t kcmd_args[KCMD_MAX_ARGS];
static size_t kcmd_count = 0;

void kcmd_parse(char *cmdline) {
    size_t n = 0;
    char *p = cmdline;

    while (*p && n < KCMD_MAX_ARGS) {
        while (*p == ' ') p++;
        if (!*p) break;

        char *key = p;
        char *val = NULL;

        while (*p && *p != ' ' && *p != '=') p++;

        if (*p == '=') {
            *p++ = '\0';
            val = p;
            while (*p && *p != ' ') p++;
        }

        if (*p) *p++ = '\0';

        kcmd_args[n++] = (kcmd_arg_t){ key, val };
    }
    
    kcmd_count = n;
}

const char *kcmd_get(const char *key) {
    for (size_t i = 0; i < kcmd_count; i++) {
        if (strcmp(kcmd_args[i].key, key) == 0)
            return kcmd_args[i].value;
    }
    return NULL;
}

bool kcmd_has(const char *key) {
    return kcmd_get(key) != NULL;
}
