#include <stdio.h>
#include <string.h>
#include <readline.h>
#include <stdlib.h>
#include <errors.h>

#include "file_cmds.h"

// what does a shell support?
// - environment manipulation (set, unset)
// - execute utilities
// - manipulate files (cp, mv, touch, unlink, mkdir etc)
// - manipulate devices?
// - communicate with kernel, show progress etc.
// - should be able to work with pty, in graphics mode.

readline_t *rl = NULL;
char cwd[256] = {0,};
char prompt[256] = {0,};

extern 
void init() {
    printf("Welcome to shell. Type 'help' for help, 'exit' to... exit\n");
    rl = init_readline();

    cwd[0] = '\0';
    getcwd(cwd, sizeof(cwd));
    strcpy(prompt, cwd);
    strcat(prompt, " $ ");
    readline_set_prompt(rl, prompt);
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
