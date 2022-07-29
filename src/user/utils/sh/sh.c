#include <stdio.h>
#include <string.h>
#include <readline.h>

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


readline_t *rl = NULL;

void init() {
    printf("Welcome to shell. Type 'exit' to... exit\n");
    rl = init_readline("dv @ shell $ ");
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
    printf("You entered: \"%s\"\n", line);
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

    printf("Shell exiting!\n");
    return 0;
}
