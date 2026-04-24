#include "../libc_internal.h"



char **environ = NULL;
static bool environ_malloced = false;


void __init_env(char **envp) {
    // called by crt0
    // the first time, we use the initial environment
    environ = envp;
    environ_malloced = false;
}


static int __find_variable(const char *name) {
    if (environ == NULL || name == NULL || name[0] == '\0') return -1;
    
    size_t len = strlen(name);
    for (int i = 0; environ[i] != NULL; i++) {
        if (strncmp(environ[i], name, len) == 0 && environ[i][len] == '=') {
            return i;
        }
    }
    return -1;
}

static int __count_variables() {
    if (environ == NULL)
        return 0;

    int count = 0;
    while (environ[count] != NULL)
        count++;

    return count;
}

char *getenv(const char *name) {
    int idx = __find_variable(name);
    if (idx == -1) return NULL;
    
    return strchr(environ[idx], '=') + 1;
}


int setenv(const char *name, const char *value, int overwrite) {
    if (name == NULL || name[0] == '\0' || strchr(name, '=') != NULL) {
        errno = EINVAL;
        return -1;
    }

    int idx = __find_variable(name);
    
    if (idx != -1 && !overwrite)
        return 0;

    size_t new_len = strlen(name) + 1 + strlen(value) + 1;
    char *new_str = malloc(new_len);
    if (!new_str)
        return -1;
    
    strcpy(new_str, name);
    strcat(new_str, "=");
    strcat(new_str, value);

    if (idx != -1) {
        // we should free the old string, if it was malloc'ed
        environ[idx] = new_str;
        return 0;
    }

    // we need to add to the array
    int count = __count_variables();

    char **new_env = malloc((count + 1  + 1) * sizeof(char *));
    if (!new_env) {
        free(new_str);
        return -1;
    }

    if (environ) {
        memcpy(new_env, environ, sizeof(char *) * count);
    }
    
    new_env[count] = new_str;
    new_env[count + 1] = NULL;

    if (environ_malloced)
        free(environ);
    
    environ = new_env;
    environ_malloced = true;
    return 0;
}


int unsetenv(const char *name) {
    if (name == NULL || name[0] == '\0' || strchr(name, '=') != NULL) {
        errno = EINVAL;
        return -1;
    }

    int i = __find_variable(name);
    if (i == -1)
        return 0;

    // shift everything after the match up by one slot
    // this overwrites the target and brings the NULL terminator forward
    while (environ[i] != NULL) {
        environ[i] = environ[i + 1];
        i++;
    }
    
    return 0;
}

