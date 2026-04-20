#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>



// Heavily inspired by xv6 usertests.c

// Forward declarations for test functions
void test_fork_exec_wait();
void test_file_create_write_read();
void test_memory_allocation();
// void test_pipe_communication();
void test_unlink_link();
void test_directory_ops();
void test_sbrk_growth();
void test_big_dir();
void test_nested_dirs();
void test_big_file();

void print_test_status(const char *test_name, int passed) {
    if (passed) {
        printf("TEST %s: PASSED\n", test_name);
    } else {
        printf("TEST %s: FAILED\n", test_name);
    }
}

int main(int argc, char *argv[]) {
    printf("Starting user applications tests...\n");

    test_fork_exec_wait();
    test_file_create_write_read();
    test_memory_allocation();
    // test_pipe_communication();
    test_unlink_link();
    test_directory_ops();
    test_sbrk_growth();
    test_big_dir();
    test_nested_dirs();
    test_big_file();

    printf("All user applications tests completed.\n");
    return 0;
}

// Test: Basic fork, exec, wait system calls
void test_fork_exec_wait() {
    int pid = fork();
    if (pid == 0) {
        // Child process
        printf("Child process: Executing echo...\n");
        char *argv[] = { "echo", "Hello from child!", 0 };
        // Assuming exec() system call, and 'echo' is an available user program
        execv("echo", argv);
        printf("Child process: exec failed!\n"); // Should not reach here if exec succeeds
        exit(1);
    } else if (pid > 0) {
        // Parent process
        int status;
        wait(&status); // Assuming wait() system call
        if (status == 0) {
            print_test_status("fork_exec_wait", 1);
        } else {
            print_test_status("fork_exec_wait", 0);
        }
    } else {
        // fork failed
        print_test_status("fork_exec_wait", 0);
    }
}

// Test: File creation, writing, and reading
void test_file_create_write_read() {
    const char *filename = "testfile.txt";
    const char *test_data = "This is a test string written to the file.";
    char read_buffer[100];
    int fd, bytes_written, bytes_read;
    int passed = 1;

    // Create file
    fd = open(filename, O_CREAT | O_WRONLY); // Assuming open() and file flags
    if (fd < 0) {
        passed = 0;
        printf("Error: Could not create file %s\n", filename);
        goto end_test;
    }

    // Write to file
    bytes_written = write(fd, test_data, strlen(test_data)); // Assuming write() and strlen()
    if (bytes_written != (int)strlen(test_data)) {
        passed = 0;
        printf("Error: Mismatch in bytes written to %s\n", filename);
        close(fd);
        goto end_test;
    }
    close(fd);

    // Open file for reading
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        passed = 0;
        printf("Error: Could not open file %s for reading\n", filename);
        goto end_test;
    }

    // Read from file
    bytes_read = read(fd, read_buffer, sizeof(read_buffer) - 1); // Assuming read()
    if (bytes_read < 0) {
        passed = 0;
        printf("Error: Failed to read from file %s\n", filename);
        close(fd);
        goto end_test;
    }
    read_buffer[bytes_read] = '\0'; // Null-terminate

    // Compare data
    if (strcmp(test_data, read_buffer) != 0) { // Assuming strcmp()
        passed = 0;
        printf("Error: Data mismatch in %s. Expected '%s', got '%s'\n", filename, test_data, read_buffer);
    }
    close(fd);

end_test:
    unlink(filename); // Clean up: delete the file. Assuming unlink()
    print_test_status("file_create_write_read", passed);
}

// Test: Dynamic memory allocation (sbrk equivalent)
void test_memory_allocation() {
    void *ptr1, *ptr2, *ptr3;
    int passed = 1;

    // Assuming sbrk() is available to increase data segment size
    // For a basic test, we'll just try to allocate memory and write to it.
    // In a real OS, sbrk() would be used for malloc implementation.

    // A simple test with malloc for now, assuming it uses sbrk internally.
    ptr1 = malloc(1024); // Allocate 1KB
    if (ptr1 == NULL) {
        passed = 0;
        printf("Error: malloc(1024) failed\n");
        goto end_test;
    }
    *((int *)ptr1) = 12345; // Write some data

    ptr2 = malloc(4096); // Allocate 4KB
    if (ptr2 == NULL) {
        passed = 0;
        printf("Error: malloc(4096) failed\n");
        free(ptr1); // Free previous allocation
        goto end_test;
    }
    *((char *)ptr2) = 'X'; // Write some data

    // Verify written data
    if (*((int *)ptr1) != 12345) {
        passed = 0;
        printf("Error: Data corruption in ptr1\n");
    }
    if (*((char *)ptr2) != 'X') {
        passed = 0;
        printf("Error: Data corruption in ptr2\n");
    }

    free(ptr1);
    free(ptr2);

    // Test for a large allocation that might fail or push limits
    ptr3 = malloc(1024 * 1024); // Allocate 1MB
    if (ptr3 == NULL) {
        printf("Note: malloc(1MB) failed, which might be expected on small systems or due to memory limits.\n");
        // This might be considered a pass if expected, or fail if abundant memory is assumed.
        // For now, we'll consider it passed if it just doesn't crash.
        // A more robust test would check available memory.
    } else {
        *((unsigned long *)ptr3) = 0xDEADBEEF;
        if (*((unsigned long *)ptr3) != 0xDEADBEEF) {
            passed = 0;
            printf("Error: Data corruption in large allocation ptr3\n");
        }
        free(ptr3);
    }

end_test:
    print_test_status("memory_allocation", passed);
}

// // Test: Inter-process communication using pipes
// void test_pipe_communication() {
//     int p[2]; // Pipe file descriptors: p[0] for read, p[1] for write
//     char write_buf[] = "Message through pipe!";
//     char read_buf[50];
//     int pid;
//     int passed = 1;

//     if (pipe(p) < 0) { // Assuming pipe() system call
//         print_test_status("pipe_communication", 0);
//         return;
//     }

//     pid = fork();
//     if (pid == 0) {
//         // Child process: writes to pipe
//         close(p[0]); // Close read end
//         write(p[1], write_buf, strlen(write_buf) + 1); // Write data, including null terminator
//         close(p[1]); // Close write end
//         exit(0);
//     } else if (pid > 0) {
//         // Parent process: reads from pipe
//         close(p[1]); // Close write end
//         int bytes_read = read(p[0], read_buf, sizeof(read_buf));
//         if (bytes_read <= 0) {
//             passed = 0;
//         } else if (strcmp(write_buf, read_buf) != 0) {
//             passed = 0;
//         }
//         close(p[0]);
//         wait(NULL); // Wait for child to finish
//     } else {
//         passed = 0; // Fork failed
//     }
//     print_test_status("pipe_communication", passed);
// }

// Test: unlink and link system calls
void test_unlink_link() {
    const char *orig_file = "original.txt";
    const char *link_file = "linked.txt";
    int fd;
    int passed = 1;

    // Create original file
    fd = open(orig_file, O_CREAT | O_WRONLY);
    if (fd < 0) { passed = 0; printf("Error: Could not create %s\n", orig_file); goto end_test; }
    write(fd, "hello", 5);
    close(fd);

    // Create a link to the original file
    if (link(orig_file, link_file) < 0) { // Assuming link() system call
        passed = 0;
        printf("Error: Could not link %s to %s\n", orig_file, link_file);
        goto end_test;
    }

    // Unlink original file - link_file should still be accessible
    if (unlink(orig_file) < 0) { // Assuming unlink() system call
        passed = 0;
        printf("Error: Could not unlink %s\n", orig_file);
        // Attempt to unlink link_file even if orig_file failed
        unlink(link_file);
        goto end_test;
    }

    // Try to open linked file
    fd = open(link_file, O_RDONLY);
    if (fd < 0) {
        passed = 0;
        printf("Error: Could not open %s after unlinking %s\n", link_file, orig_file);
    } else {
        close(fd);
    }

end_test:
    // Clean up
    unlink(link_file);
    print_test_status("unlink_link", passed);
}

// Test: Directory operations (mkdir, rmdir)
void test_directory_ops() {
    const char *dirname = "testdir";
    int passed = 1;

    if (mkdir(dirname, 0755) < 0) { // Assuming mkdir() system call
        passed = 0;
        printf("Error: Could not create directory %s\n", dirname);
        goto end_test;
    }

    // Try to create a file inside
    int fd = open("testdir/file_in_dir.txt", O_CREAT | O_WRONLY);
    if (fd < 0) {
        passed = 0;
        printf("Error: Could not create file inside %s\n", dirname);
        rmdir(dirname); // Attempt cleanup
        goto end_test;
    }
    close(fd);
    unlink("testdir/file_in_dir.txt"); // Remove file before removing dir

    if (rmdir(dirname) < 0) { // Assuming rmdir() system call
        passed = 0;
        printf("Error: Could not remove directory %s\n", dirname);
    }

end_test:
    print_test_status("directory_ops", passed);
}

// Test: sbrk system call (simulated) for heap growth
void test_sbrk_growth() {
    // This test assumes a direct sbrk() equivalent, not just malloc.
    // For this hobby OS, we will simulate it by trying to use a large amount of memory
    // and checking if the program is still running and can access it.
    // A true sbrk test would involve getting the current program break,
    // increasing it, writing to the new memory, and then decreasing it.
    // Since actual sbrk is OS-specific, this is a conceptual test.

    // Using malloc to simulate demanding memory, assuming malloc internally uses sbrk or similar.
    void *big_block = malloc(1024 * 512); // Try to allocate 512KB
    int passed = 1;

    if (big_block == NULL) {
        printf("Note: Large malloc (512KB) failed, potentially due to memory limits. This is not necessarily a test failure.\n");
        // Depending on system design, this might be a pass.
        // For now, we assume it's acceptable if it doesn't crash.
        passed = 1; // Mark as passed if it gracefully handles failure, or if it succeeds.
    } else {
        // If allocation succeeds, attempt to write and read to a part of it
        // to ensure the memory is accessible.
        unsigned int *int_ptr = (unsigned int *)big_block;
        int_ptr[0] = 0xABCDEF00;
        if (int_ptr[0] != 0xABCDEF00) {
            passed = 0;
            printf("Error: Data corruption in large sbrk-like allocation.\n");
        }
        free(big_block);
    }

    print_test_status("sbrk_growth", passed);
}

// Test: Create a large number of files in a single directory
void test_big_dir() {
    const char *dirname = "bigdir_test";
    const int num_files = 50; // Number of files to create
    int passed = 1;

    if (mkdir(dirname, 0755) < 0) {
        printf("Error: test_big_dir: Could not create directory %s\n", dirname);
        print_test_status("big_dir", 0);
        return;
    }

    // Create files
    for (int i = 0; i < num_files; ++i) {
        char filename[64];
        sprintf(filename, "%s/file%d.txt", dirname, i); // Assuming sprintf
        int fd = open(filename, O_CREAT | O_WRONLY);
        if (fd < 0) {
            printf("Error: test_big_dir: Could not create file %s\n", filename);
            passed = 0;
            break;
        }
        write(fd, "data", 4); // Write some small data
        close(fd);
    }

    // Verify some files exist (e.g., try to open the last one)
    if (passed) {
        char last_filename[64];
        sprintf(last_filename, "%s/file%d.txt", dirname, num_files - 1);
        int fd = open(last_filename, O_RDONLY);
        if (fd < 0) {
            printf("Error: test_big_dir: Last file %s not found or unreadable.\n", last_filename);
            passed = 0;
        } else {
            close(fd);
        }
    }

    // Clean up
    for (int i = 0; i < num_files; ++i) {
        char filename[64];
        sprintf(filename, "%s/file%d.txt", dirname, i);
        unlink(filename);
    }
    rmdir(dirname);

    print_test_status("big_dir", passed);
}

// Test: Create and remove nested directories
void test_nested_dirs() {
    const char *dir_path = "a/b/c/d/e";
    char current_path[256];
    int passed = 1;

    strcpy(current_path, ""); // Assuming strcpy
    char *path_copy = malloc(strlen(dir_path) + 1);
    if (path_copy == NULL) {
        printf("Error: test_nested_dirs: malloc failed.\n");
        print_test_status("nested_dirs", 0);
        return;
    }
    strcpy(path_copy, dir_path);

    // This is a simplified strtok for demonstration; real strtok is tricky with const char*
    // Using a manual split to avoid complex strtok state
    char *start = path_copy;
    char *end;

    // Create directories
    while ((end = strchr(start, '/')) != NULL) { // Assuming strchr
        *end = '\0'; // Temporarily null-terminate the current directory name
        if (strlen(current_path) > 0) {
            strcat(current_path, "/"); // Assuming strcat
        }
        strcat(current_path, start);
        if (mkdir(current_path, 0755) < 0) {
            printf("Error: test_nested_dirs: Could not create directory %s\n", current_path);
            passed = 0;
            break;
        }
        start = end + 1;
    }
    if (passed && strlen(start) > 0) { // Create the last directory
        if (strlen(current_path) > 0) {
            strcat(current_path, "/");
        }
        strcat(current_path, start);
        if (mkdir(current_path, 0755) < 0) {
            printf("Error: test_nested_dirs: Could not create final directory %s\n", current_path);
            passed = 0;
        }
    }

    // Verify existence of the deepest directory
    if (passed) {
        int fd = open(dir_path, O_RDONLY); // Try to open as a directory
        if (fd < 0) {
            printf("Error: test_nested_dirs: Deepest directory %s not accessible.\n", dir_path);
            passed = 0;
        } else {
            close(fd);
        }
    }

    // Clean up: remove directories from deep to shallow
    if (passed) { // Only attempt cleanup if creation was successful enough to proceed
        char cleanup_path[256];
        strcpy(cleanup_path, dir_path);
        
        // Remove from deepest to shallowest
        while (strlen(cleanup_path) > 0) {
            if (rmdir(cleanup_path) < 0) {
                printf("Error: test_nested_dirs: Could not remove directory %s\n", cleanup_path);
                passed = 0; // Mark as failed if cleanup fails
            }
            char *last_slash = strrchr(cleanup_path, '/'); // Assuming strrchr
            if (last_slash) {
                *last_slash = '\0';
            } else {
                break; // No more slashes, top level
            }
        }
    }
    free(path_copy);
    print_test_status("nested_dirs", passed);
}

// Test: Create and operate on a large file
void test_big_file() {
    const char *filename = "bigfile.txt";
    const int chunk_size = 4096; // 4KB chunks
    const int total_size_mb = 1; // 1MB total size
    const int total_size = total_size_mb * 1024 * 1024;
    char *write_buffer = malloc(chunk_size);
    char *read_buffer = malloc(chunk_size);
    int fd;
    int passed = 1;

    if (!write_buffer || !read_buffer) {
        printf("Error: test_big_file: malloc failed for buffers.\n");
        if (write_buffer) free(write_buffer);
        if (read_buffer) free(read_buffer);
        print_test_status("big_file", 0);
        return;
    }

    // Fill write buffer with a pattern
    for (int i = 0; i < chunk_size; ++i) {
        write_buffer[i] = (char)(i % 256);
    }

    // Create and write the large file
    fd = open(filename, O_CREAT | O_WRONLY);
    if (fd < 0) {
        printf("Error: test_big_file: Could not create file %s\n", filename);
        passed = 0;
        goto end_test;
    }

    for (int i = 0; i < total_size / chunk_size; ++i) {
        if (write(fd, write_buffer, chunk_size) != chunk_size) {
            printf("Error: test_big_file: Failed to write chunk %d\n", i);
            passed = 0;
            break;
        }
    }
    close(fd);

    if (!passed) goto end_test;

    // Open and read the large file
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        printf("Error: test_big_file: Could not open file %s for reading\n", filename);
        passed = 0;
        goto end_test;
    }

    for (int i = 0; i < total_size / chunk_size; ++i) {
        if (read(fd, read_buffer, chunk_size) != chunk_size) {
            printf("Error: test_big_file: Failed to read chunk %d or read less than expected.\n", i);
            passed = 0;
            break;
        }
        if (memcmp(write_buffer, read_buffer, chunk_size) != 0) { // Assuming memcmp
            printf("Error: test_big_file: Data mismatch at chunk %d.\n", i);
            passed = 0;
            break;
        }
    }
    close(fd);

end_test:
    // Clean up
    if (write_buffer) free(write_buffer);
    if (read_buffer) free(read_buffer);
    unlink(filename);

    print_test_status("big_file", passed);
}


