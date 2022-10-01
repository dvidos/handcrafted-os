#include <stdio.h>
#include <string.h>
#include <readline.h>
#include <stdlib.h>
#include <errors.h>

// what does a shell support?
// - environment manipulation (set, unset)
// - execute utilities
// - manipulate files (cp, mv, touch, unlink, mkdir etc)
// - manipulate devices?
// - communicate with kernel, show progress etc.
// - should be able to work with pty, in graphics mode.

// file_t *cwd;
// char *cwd_name;

// void chdir(char *path) {
//     // open the new directory
//     // set the cwd name
// }

// char *getcwd() {
//     return cwd_name
// }


// // we can have stdin, stdout, stderr, file streams, memory streams, etc.
// void stream_read(stream_t *stream, char *buffer, int len) {

// }
// void stream_write(stream_t *stream, char *buffer, int len) {

// }


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
    
    char *path = argc <= 1 ? "/" : argv[1];
    syslog_trace("command ls \"%s\"", path);

    int h = opendir(path);
    if (h < 0) {
        printf("Error %d opening dir\n", h);
        return;
    }

    dir_entry_t entry;
    memset(&entry, 0, sizeof(dir_entry_t));
    syslog_info("dir handle returned = %d", h);

    int err;
    while ((err = readdir(h, &entry)) == SUCCESS) {
        syslog_info("reading success %d", err);
        printf("%c  %8d  %04d-%02d-%02d %02d:%02d:%02d  %s\n", 
            entry.flags.dir ? 'd' : 'f', 
            entry.file_size, 
            entry.modified.year,
            entry.modified.month,
            entry.modified.day,
            entry.modified.hours,
            entry.modified.minutes,
            entry.modified.seconds,
            entry.short_name);
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

struct built_in_info {
    char *name;
    void (*func)(int argc, char *argv[]);
};

struct built_in_info built_ins[] = {
    {"echo", cmd_echo},
    {"set", cmd_set},
    {"unset", cmd_unset},
    {"ls", cmd_ls},
    {"cat", cmd_cat},
    {NULL, NULL}
};

readline_t *rl = NULL;

void init() {
    printf("Welcome to shell. Type 'help' for help, 'exit' to... exit\n");
    rl = init_readline("dv @ shell $ ");
}

void fini() {
    printf("Shell exiting!\n");
}

struct run_arguments {
    char *tokens;
    int argc;
    char *argv[16];
};

void parse_run_arguments(char *line, struct run_arguments **args) {
    struct run_arguments *p = malloc(sizeof(struct run_arguments));
    memset(p, 0, sizeof(struct run_arguments));

    p->tokens = malloc(strlen(line) + 1);
    strcpy(p->tokens, line);

    char *token = strtok(p->tokens, " ");
    while (token != NULL) {
        p->argv[p->argc] = token;
        p->argc++;
        if (p->argc == 16) {
            printf("warn: 16 args limit reached\n");
            break;
        }
        token = strtok(NULL, " ");
    }

    *args = p;
}

void free_run_arguments(struct run_arguments *args) {
    free(args->tokens);
    free(args);
}


void execute_line(char *line) {
    /*
        things to support:
        - manage env variables (set, getenv() and pass env in children)
        - HOME, PATH, PS1 variables
        - variable substitution (e.g. $HOME)
        - current working directory, relative path, cd, pwd
        - execute programs, based on PATH
        - autocompletion based on files

        - file manipulation: cp, mv, rm, mkdir rmdir, touch, etc.
        - colors (parametric) and ascii for Nicholas
        - some kernel monitoring program or something similar

        Another way of thinking about shell, is that
        this is the program that allows the user to harness
        the computer and the computing power.

        i.e. it should be the highest level of programming language
        for anything the user needs to do.
        though today, users just want to... waste time on social media.
    */
    struct run_arguments *args;
    parse_run_arguments(line, &args);
    if (args->argc == 0) {
        free_run_arguments(args);
        return;
    }
    
    bool found = false;

    // run a built_in, if one exists
    struct built_in_info *info = built_ins;
    while (info->name != NULL) {
        if (strcmp(info->name, args->argv[0]) == 0) {
            // run it
            found = true;
            info->func(args->argc, args->argv);
            break;
        }
        info++;
    }

    // otherwise, try to run something from the PATH
    if (!found) {
        int h = open(args->argv[0]);
        if (h >= 0) {
            // so it is a file we can open. try to execute it.
            close(h);
            found = true;
            int err = exec(args->argv[0], args->argv, environ);
            
            if (err < 0) {
                printf("Error %d executing %s", err, args->argv[0]);
            } else {
                int exit_status = 0;
                err = wait(&exit_status);
                if (err < 0)
                    printf("Error %d waiting for child\n", err);
                else {
                    int child_pid = err;
                    //if (exit_status != 0)
                    printf("\nChild PID %d exited with exit code %d\n", child_pid, exit_status);
                }
            }
        }
    }

    if (!found) {
        printf("%s: command not found\n", args->argv[0]);
    }

    free_run_arguments(args);
}

int main(int argc, char *argv[], char *envp[]) {
    (void)argc;
    (void)argv;
    (void)envp;
    
    init();
    while (true) {
        char *line = readline(rl);
        if (strcmp(line, "exit") == 0)
            break;
        
        execute_line(line);
    }

    fini();
    return 0;
}
