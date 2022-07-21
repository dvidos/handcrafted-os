

// what does a shell support?
// - environment manipulation (set, unset)
// - execute utilities
// - manipulate files (cp, mv, touch, unlink, mkdir etc)
// - manipulate devices?
// - communicate with kernel, show progress etc.
// - should be able to work with pty, in graphics mode.

int main(int argc, char *argv[], char *envp[]) {

    while (true) {
        readline();
        if (EOF)
            break;
        
        execlice();
    }
}