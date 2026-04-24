#include "../libc_internal.h"


// Forward declaration for the internal execve syscall wrapper
extern int execve(const char *path, char *const argv[], char *const envp[]);
extern char **environ;

// Helper function to check if a file exists and is executable
static bool is_executable(const char *path) {
    // F_OK checks for existence, X_OK checks for execute permission
    return access(path, F_OK | X_OK) == 0;
}

// Helper function to handle shebang and execute
static int _execvp_impl(const char *path, char *const argv[], char *const envp[]) {
    // Before trying to open, check if it's executable via access().
    // This is important because fopen() on a directory or non-readable file
    // would also fail, and we want to distinguish "not executable" from "shebang script".
    if (!is_executable(path)) {
        // If it's not executable, execve would fail anyway with EACCES or similar.
        // We let execve handle the final error reporting.
        // However, if we know it's not executable, we can short-circuit.
        // But for shebang scripts, access(X_OK) might fail if the script itself isn't
        // marked executable but its interpreter is. The kernel's execve should
        // handle the permissions check correctly for shebangs.
        // So, we proceed assuming execve will do the right thing for non-executable
        // files or files that are directories.
        // For direct execution, we check. For shebang, we try to read.
        // If it's a directory, fopen will fail, and execve will get ENOENT/EACCES.
    }


    // Try to open the file to check for shebang
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        // If we can't open it (e.g., doesn't exist, permission denied, is a directory),
        // execve will likely return an appropriate error.
        // We will call execve directly.
        return execve(path, argv, envp);
    }

    char first_line[256]; // Max length for shebang line
    if (fgets(first_line, sizeof(first_line), f) == NULL) {
        fclose(f);
        // Empty file or read error, execve will handle it
        return execve(path, argv, envp);
    }
    fclose(f);

    if (first_line[0] == '#' && first_line[1] == '!') {
        // Shebang detected
        char *interpreter_path = NULL;
        char *interpreter_arg = NULL;
        
        char *line_ptr = first_line + 2; // Skip '#!'

        // Trim leading whitespace
        while (*line_ptr == ' ' || *line_ptr == '\t') {
            line_ptr++;
        }

        char *end_of_interp = line_ptr;
        while (*end_of_interp != ' ' && *end_of_interp != '\t' && *end_of_interp != '\n' && *end_of_interp != '\0') {
            end_of_interp++;
        }
        
        size_t interp_len = end_of_interp - line_ptr;
        if (interp_len == 0) {
            // Invalid shebang, no interpreter specified
            errno = ENOEXEC; // Exec format error
            return -1;
        }

        interpreter_path = (char *)malloc(interp_len + 1);
        if (interpreter_path == NULL) {
            errno = ENOMEM;
            return -1;
        }
        strncpy(interpreter_path, line_ptr, interp_len);
        interpreter_path[interp_len] = '\0';

        char *after_interp = end_of_interp;
        while (*after_interp == ' ' || *after_interp == '\t') {
            after_interp++;
        }

        // Check for interpreter argument
        char *end_of_arg = after_interp;
        while (*end_of_arg != '\n' && *end_of_arg != '\0') {
            end_of_arg++;
        }
        
        size_t arg_len = end_of_arg - after_interp;
        if (arg_len > 0) {
            interpreter_arg = (char *)malloc(arg_len + 1);
            if (interpreter_arg == NULL) {
                free(interpreter_path);
                errno = ENOMEM;
                return -1;
            }
            strncpy(interpreter_arg, after_interp, arg_len);
            interpreter_arg[arg_len] = '\0';
        }

        // Construct new argv for the interpreter
        int original_argc = 0;
        if (argv != NULL) {
            while (argv[original_argc] != NULL) {
                original_argc++;
            }
        }
        
        // New argv: [interpreter_path, interpreter_arg (optional), path_to_script, original_argv[1], ...]
        // Count total arguments for new_argv
        int new_argc = 0;
        // The first argument is the interpreter path itself (argv[0] for the interpreter)
        if (interpreter_path != NULL) new_argc++; 
        // If there's an interpreter_arg, that's argv[1] for the interpreter
        if (interpreter_arg != NULL) new_argc++;
        new_argc++; // for path_to_script (argv[1] or argv[2] for the interpreter)
        new_argc += (original_argc > 1 ? original_argc - 1 : 0); // rest of original argv
        new_argc++; // for NULL terminator

        char **new_argv = (char **)malloc(sizeof(char *) * new_argc);
        if (new_argv == NULL) {
            free(interpreter_path);
            if (interpreter_arg) free(interpreter_arg);
            errno = ENOMEM;
            return -1;
        }

        int current_arg_idx = 0;

        // Interpreter path is the first argument for the interpreter
        if (interpreter_path != NULL) {
            new_argv[current_arg_idx++] = interpreter_path;
        }
        // Interpreter argument comes next if present
        if (interpreter_arg != NULL) {
            new_argv[current_arg_idx++] = interpreter_arg;
        }
        // The script itself becomes an argument to the interpreter
        new_argv[current_arg_idx++] = (char *)path; 

        // Copy remaining arguments from original argv
        for (int i = 1; i < original_argc; i++) {
            new_argv[current_arg_idx++] = argv[i];
        }
        new_argv[current_arg_idx] = NULL;
        
        int ret = execve(interpreter_path, new_argv, envp);

        // Free allocated memory on failure (if execve returns)
        free(interpreter_path);
        if (interpreter_arg) free(interpreter_arg);
        free(new_argv);
        
        return ret;

    } else {
        // Not a shebang script, execute directly (after checking executability)
        // If it's a regular file, check if it's executable.
        // If it's a directory, access(path, X_OK) will return -1.
        if (!is_executable(path)) {
            // If it's a regular file but not executable, set EACCES
            // (assuming access() correctly differentiates)
            // execve will return EACCES or similar.
            // But we should ensure errno is set appropriately if access fails here.
            // However, execve will do its own checks anyway.
            // Let's rely on execve for the final decision.
        }
        return execve(path, argv, envp);
    }
}


int execvp(const char *file, char *const argv[]) {
    char *path_env = getenv("PATH");
    char *const *envp = environ; // Use global environ

    if (file == NULL || file[0] == '\0') {
        errno = ENOENT; // No such file or directory
        return -1;
    }

    // If file contains a slash, it's a direct path or relative path, no PATH search
    if (strchr(file, '/') != NULL) {
        return _execvp_impl(file, argv, envp);
    }

    // If PATH is not set or empty, try current directory only.
    // POSIX specifies that if PATH is unset, the behavior is implementation-defined,
    // but often it means searching the current directory.
    // If PATH is explicitly empty, it generally means no search path.
    if (path_env == NULL || path_env[0] == '\0') {
        // According to POSIX, if `file` does not contain a slash and PATH
        // is unset or null, execvp should search in the current directory only.
        // However, if PATH is set to "", it should *not* search the current directory.
        // For simplicity and matching common Unix behavior, if PATH is unset/empty,
        // we'll try the current directory. If that's not desired, the system
        // administrator can set PATH to something like "/bin:/usr/bin".
        char current_dir_path[256]; // Assuming MAX_PATH
        strcpy(current_dir_path, "./"); // Prepend "./"
        strcat(current_dir_path, file);
        return _execvp_impl(current_dir_path, argv, envp);
    }

    // Duplicate path_env because strtok_r modifies it
    char *path_copy = strdup(path_env);
    if (path_copy == NULL) {
        errno = ENOMEM;
        return -1;
    }

    char *dir;
    char *rest = path_copy;
    int ret = -1;
    
    // Iterate through PATH directories
    // strtok_r is reentrant and safer for multithreaded environments
    // The loop condition is dir = strtok_r(NULL, ":", &rest) after the first call
    // or dir = strtok_r(path_copy, ":", &rest) initially.
    char *token_ptr = path_copy;

    while ((dir = strtok_r(token_ptr, ":", &rest))) {
        token_ptr = NULL; // For subsequent calls to strtok_r

        char full_path[1024]; // Assuming MAX_PATH is sufficiently large
        // Check for buffer overflow before concatenation
        if (strlen(dir) + strlen(file) + 2 > sizeof(full_path)) { // +2 for '/' and null terminator
            // Path too long, skip this directory and try the next one
            continue;
        }

        strcpy(full_path, dir);
        strcat(full_path, "/");
        strcat(full_path, file);

        // Check if the file exists and is executable *before* trying to execve.
        // This is important for execvp to correctly set errno to ENOENT vs EACCES.
        if (!is_executable(full_path)) {
            // If it's not executable or doesn't exist, continue searching PATH.
            // access() will set errno. We only want to stop searching PATH
            // if it's executable or if the error is something other than ENOENT/ENOTDIR.
            // If access fails with ENOENT/ENOTDIR, we should keep searching.
            // If it fails with EACCES (exists but not executable), we should keep searching
            // unless the POSIX standard dictates otherwise for execvp.
            // For now, if access fails, we assume it's not the right candidate and continue.
            // execve will perform its own, more thorough checks.
            continue;
        }

        ret = _execvp_impl(full_path, argv, envp);
        
        // If _execvp_impl returns, it means execution failed.
        // execvp should return -1 and set errno if it failed for the *final* path.
        // If _execvp_impl returned, we know it failed.
        // However, if the error is ENOENT or ENOTDIR, it means the file wasn't found at that
        // location (or was a directory), so we should continue searching PATH.
        // If it's another error (like EACCES, ENOEXEC), it means we found the file
        // but couldn't execute it for other reasons, so we should report that error
        // and stop searching.
        if (ret == -1) {
            // Check errno from _execvp_impl (which comes from execve or shebang parsing)
            if (errno == ENOENT || errno == ENOTDIR) {
                // Keep searching PATH for the file
                continue;
            } else {
                // Found the file, but couldn't execute for other reasons. Propagate error.
                free(path_copy);
                return -1;
            }
        } else {
            // execve succeeded, execvp does not return.
            // This code should ideally not be reached if execve works as expected.
            free(path_copy);
            return 0; // Should not happen
        }
    }

    free(path_copy);
    
    // If we reach here, no executable was found in PATH, or all found candidates
    // failed with ENOENT/ENOTDIR (in which case the last errno is ENOENT/ENOTDIR).
    // If the loop completed, it means all attempts failed with ENOENT/ENOTDIR or no path was found.
    // So, we set errno to ENOENT as per POSIX for 'command not found'.
    errno = ENOENT;
    return -1;
}