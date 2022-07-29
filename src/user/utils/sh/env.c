#include <ctypes.h>
#include <string.h>
#include <stdlib.h>


char **envp;


void initenv() {
    envp = malloc(sizeof(char *));
    *envp = NULL;
}

void setenv(char *varname, char *value) {

    char **p = envp;
    size_t varlen = strlen(varname);

    while (*p != NULL) {
        if (memcmp(*p, varname, varlen) == 0 && (*p)[varlen] == '=') {
            size_t len = varlen + 1 + strlen(value);
            if (strlen(*p) != len) {
                free(*p);
                *p = malloc(len + 1);
            }
            strcpy(*p, varname);
            strcat(*p, "=");
            strcat(*p, value);
            return;
        }
        p++;
    }

    // we did not find it, let's count entries
    int entries = 0;
    p = envp;
    while (*p != NULL) {
        entries++;
        p++;
    }
    
    // allocate new table and copy entries
    char **new_env = malloc(sizeof(char *) * (entries + 2));
    p = envp;
    char **dest = new_env;
    while (*p != NULL) {
        *dest = *p;
        p++;
        dest++;
    }

    // append our new entry and NULL terminator
    *dest = malloc(varlen + 1 + strlen(value) + 1);
    strcpy(*dest, varname);
    strcat(*dest, "=");
    strcat(*dest, value);
    dest++;
    *dest = NULL;

    // free and assign the variable
    free(envp);
    envp = new_env;
}

char *getenv(char *varname) {

    char **p = envp;
    int varlen = strlen(varname);

    while (*p != NULL) {
        if (memcmp(*p, varname, varlen) == 0 && (*p)[varlen] == '=')
            return *p;
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

    char **p = envp;
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
}

