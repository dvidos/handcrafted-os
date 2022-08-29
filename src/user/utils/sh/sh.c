#include <stdio.h>
#include <string.h>
#include <readline.h>
#include <stdlib.h>

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

struct built_in_info {
    char *name;
    void (*func)(int argc, char *argv[]);
};

struct built_in_info built_ins[] = {
    { "echo", cmd_echo},
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
            printf("warn: 16 args limit reached");
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


void exec(char *line) {
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
        
        exec(line);
    }

    fini();
    return 0;
}
