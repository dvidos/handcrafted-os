#include <stdio.h>
#include <stdlib.h>

struct built_in_info {
    char *name;
    void (*func)(int argc, char *argv[]);
};


void cmd_help(int argc, char *argv[]) {
    printf("- built in commands: ls, cd, cat, echo, cp, mv, mkdir, rmdir, touch, rm\n");
    printf("- set: to view or set variables\n");
    printf("- correct parsing of double quotes in arguments\n");
    printf("- ability to substitute variables\n");
    printf("- ability to expand wildcard files\n");
    printf("- ability to auto complete file names\n");
}

void cmd_echo(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        printf("%s ", argv[i]);
    }
    
    // for normal unix, "-n" suppresses the new line character
    printf("\n");
}

void cmd_set(int argc, char *argv[]) {
    if (argc <= 1) {
        // show all
        char **p = environ;
        while (*p != NULL) {
            printf("%s\n", *p);
            p++;
        }

    } else {
        for (int i = 1; i < argc; i++) {
            // need to split the "=" part
            char *equal = strchr(argv[i], '=');
            char *name;
            char *value;
            if (equal == NULL) {
                name = "_";
                value = argv[i];
            } else {
                *equal = '\0';
                name = argv[i];
                value = equal + 1;
            }
            setenv(name, value);
        }
    }
}

void cmd_unset(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        unsetenv(argv[i]);
    }
}

void cmd_ls(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    char *path = argc <= 1 ? "." : argv[1];
    syslog_trace("command ls \"%s\"", path);

    int h = opendir(path);
    if (h < 0) {
        printf("Error %d opening dir\n", h);
        return;
    }

    dirent_t *ent;

    int err;
    while ((ent = readdir(h)) != NULL) {
        printf("%c  %8d  %s\n", ent->type == 2 ? 'd' : 'f', ent->size, ent->name);
    }
    syslog_info("closing handle %d", h);
    err = closedir(h);
    if (err < 0) {
        printf("Error %d closing dir\n", err);
    }
}

void cmd_cat(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    if (argc <= 1) {
        printf("Usage: cat <file>\n");
        return;
    }

    syslog_info("opening file \"%s\"", argv[1]);
    int h = open(argv[1]);
    if (h < 0) {
        printf("Error %d opening %s\n", h, argv[1]);
        return;
    }

    char buffer[64 + 1];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = read(h, buffer, sizeof(buffer) - 1);
        printf("%s", buffer);

        if (bytes < (int)(sizeof(buffer) - 1))
            break;
    }
    printf("\n");
    close(h);
}

void cmd_cd(int argc, char *argv[]) {
    int err;

    if (argc <= 1) {
        int err = getcwd(cwd, sizeof(cwd));
        if (err) {
            printf("Error %d getting current working directory\n");
        } else {
            puts(cwd);
            printf("\n");
        }
    } else {
        err = chdir(argv[1]);
        if (err) {
            printf("Error %d changing directory\n", err);
        } else {
            cwd[0] = '\0';
            getcwd(cwd, sizeof(cwd));
            strcpy(prompt, cwd);
            strcat(prompt, " $ ");
            readline_set_prompt(rl, prompt);
        }
    }
}

void cmd_touch(int argc, char *argv[]) {
    bool verbose = false;
    
    // maybe we should support a "-v" for verbose switch?
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'v')
                verbose = true;
        } else {
            if (verbose)
                printf("Touching \"%s\"... ", argv[i]);

            int err = touch(argv[i]);
            if (err)
                printf("Error %d touching %s\n", argv[i]);

            if (verbose)
                printf("\n");
        }
    }
}

void cmd_rm(int argc, char *argv[]) {
    bool verbose = false;
    
    // maybe we should support a "-v" for verbose switch?
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'v')
                verbose = true;
        } else {
            if (verbose)
                printf("Unlinking \"%s\"... ", argv[i]);

            int err = unlink(argv[i]);
            if (err)
                printf("Error %d unlinking %s\n", argv[i]);

            if (verbose)
                printf("\n");
        }
    }
}

void cmd_mkdir(int argc, char *argv[]) {
    bool verbose = false;
    
    // maybe we should support a "-v" for verbose switch?
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'v')
                verbose = true;
        } else {
            if (verbose)
                printf("Creating directory \"%s\"... ", argv[i]);

            int err = mkdir(argv[i]);
            if (err)
                printf("Error %d creating directory %s\n", argv[i]);

            if (verbose)
                printf("\n");
        }
    }
}

void cmd_rmdir(int argc, char *argv[]) {
    bool verbose = false;
    
    // maybe we should support a "-v" for verbose switch?
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'v')
                verbose = true;
        } else {
            if (verbose)
                printf("Removing directory \"%s\"... ", argv[i]);

            // empty check should be a vfs issue, no?
            int err = rmdir(argv[i]);
            if (err)
                printf("Error %d removing directory %s\n", argv[i]);

            if (verbose)
                printf("\n");
        }
    }
}

void cmd_cp(int argc, char *argv[]) {
}

void cmd_mv(int argc, char *argv[]) {
}

struct built_in_info built_ins[] = {
    {"help", cmd_help},
    {"cd", cmd_cd},
    {"echo", cmd_echo},
    {"set", cmd_set},
    {"unset", cmd_unset},
    {"ls", cmd_ls},
    {"cat", cmd_cat},
    {"touch", cmd_touch},
    {"rm", cmd_rm},
    {"mkdir", cmd_mkdir},
    {"rmdir", cmd_rmdir},
    {"cp", cmd_cp},
    {"mv", cmd_mv},
    {NULL, NULL}
};

