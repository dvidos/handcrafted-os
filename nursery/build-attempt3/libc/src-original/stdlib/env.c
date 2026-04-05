#include <ctypes.h>
#include <string.h>
#include <stdlib.h>


char **environ;
static bool environ_modified;


void __init_env(char **envp) {
    // the first time, we use the initial environment
    environ = envp;
    environ_modified = false;
}

void setenv(char *varname, char *value) {

    char **p = environ;
    size_t varlen = strlen(varname);

    // see if it already exists
    while (*p != NULL) {
        if (memcmp(*p, varname, varlen) == 0 && (*p)[varlen] == '=') {
            // we have found the entry, change in place, or reallocate
            size_t current_len = strlen(*p);
            size_t needed_len = varlen + 1 + strlen(value);
            if (needed_len > current_len) {
                // reallocate only if insufficient space
                free(*p);
                *p = malloc(needed_len + 1);
            }

            strcpy(*p, varname);
            strcat(*p, "=");
            strcat(*p, value);
            break;

        } else {
            p++;
            continue;
        }
    }

    // a non-null p means we found the entry
    if (*p != NULL)
        return;

    // let's count entries to reallocate a new array
    int entries = 0;
    for (p = environ; *p != NULL; p++)
        entries++;
    char **new_env = malloc(sizeof(char *) * (entries + 2));
    
    // allocate new table and copy entries
    p = environ;
    char **new_p = new_env;
    while (*p != NULL) {
        *new_p = *p;
        p++;
        new_p++;
    }

    // append our new entry
    *new_p = malloc(varlen + 1 + strlen(value) + 1);
    strcpy(*new_p, varname);
    strcat(*new_p, "=");
    strcat(*new_p, value);

    // append the NULL pointer at the end
    new_p++;
    *new_p = NULL;

    // free old array, if it was allocated
    if (environ_modified)
        free(environ);
    
    // set the new values
    environ = new_env;
    environ_modified = true;
}

char *getenv(char *varname) {

    char **p = environ;
    int varlen = strlen(varname);

    while (*p != NULL) {
        if (memcmp(*p, varname, varlen) == 0 && (*p)[varlen] == '=')
            return (*p) + varlen + 1;
        p++;
    }

    return NULL;
}

void unsetenv(char *varname) {
    // find the variable, then make the array pointer NULL
    // shift the other entries up, as a NULL entry always means end of 
    // empty string is fine, NULL is not accepted, use unsetenv()
    // if entry exists, if same size, change in place
    // if entry exists, larges size, allocate new string, update pointer.
    // if more entries are needed, allocate new array, update copy existing pointers, append
    // always leave one NULL pointer in the array!

    char **p = environ;
    int varlen = strlen(varname);

    bool found = false;
    while (*p != NULL) {
        if (memcmp(*p, varname, varlen) == 0 && (*p)[varlen] == '=')
            found = true;
        // after we pass the to-delete entry, bring things one entry higher
        if (found)
            *p = *(p + 1);
        p++;
    }
    // copy the final NULL pointer
    *p = *(p + 1);
}

