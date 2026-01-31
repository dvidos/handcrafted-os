#include <ctypes.h>
#include <errors.h>
#include <memory/kheap.h>
#include <klib/path.h>
#include <klib/string.h>
#include "framework.h"


void test_paths() {

    assert(count_path_parts("") == 0);
    assert(count_path_parts("/") == 0);

    assert(count_path_parts("/abc") == 1);
    assert(count_path_parts("/abc/") == 1);
    assert(count_path_parts("abc/") == 1);

    assert(count_path_parts("abc/def") == 2);
    assert(count_path_parts("/abc/def") == 2);
    assert(count_path_parts("abc/def/") == 2);

    assert(count_path_parts("/use/share/lib/kernel/klibc.a") == 5);
    assert(count_path_parts("/home/even with spaces") == 2);

    char *path = "/usr/share/lib/kernel/klibc.a";
    assert(count_path_parts(path) == 5);

    char buffer[32];
    int offset = 0;
    int err;
    
    err = get_next_path_part(path, &offset, buffer);
    assert(err == SUCCESS);
    assert(strcmp(buffer, "usr") == 0);
    
    err = get_next_path_part(path, &offset, buffer);
    assert(err == SUCCESS);
    assert(strcmp(buffer, "share") == 0);
    
    err = get_next_path_part(path, &offset, buffer);
    assert(err == SUCCESS);
    assert(strcmp(buffer, "lib") == 0);
    
    err = get_next_path_part(path, &offset, buffer);
    assert(err == SUCCESS);
    assert(strcmp(buffer, "kernel") == 0);
    
    err = get_next_path_part(path, &offset, buffer);
    assert(err == SUCCESS);
    assert(strcmp(buffer, "klibc.a") == 0);
    
    err = get_next_path_part(path, &offset, buffer);
    assert(err == ERR_NO_MORE_CONTENT);

    // now, specific ones
    err = get_n_index_path_part(path, 5, buffer);
    assert(err == ERR_NOT_FOUND);

    err = get_n_index_path_part(path, 4, buffer);
    assert(err == SUCCESS);
    assert(strcmp(buffer, "klibc.a") == 0);

    err = get_n_index_path_part(path, 3, buffer);
    assert(err == SUCCESS);
    assert(strcmp(buffer, "kernel") == 0);

    err = get_n_index_path_part(path, 2, buffer);
    assert(err == SUCCESS);
    assert(strcmp(buffer, "lib") == 0);

    err = get_n_index_path_part(path, 1, buffer);
    assert(err == SUCCESS);
    assert(strcmp(buffer, "share") == 0);

    err = get_n_index_path_part(path, 0, buffer);
    assert(err == SUCCESS);
    assert(strcmp(buffer, "usr") == 0);


    // for expectations see https://linux.die.net/man/3/dirname

    char *p = strdup("/usr/home/file.txt");
    assert(strcmp(dirname(p), "/usr/home") == 0);
    kfree(p);

    p = strdup("/usr/home/file.txt");
    assert(strcmp(pathname(p), "file.txt") == 0);
    kfree(p);

    p = strdup("/usr/var/log/");
    assert(strcmp(dirname(p), "/usr/var") == 0);
    kfree(p);

    p = strdup("/usr/var/log/");
    assert(strcmp(pathname(p), "log") == 0);
    kfree(p);

    p = strdup("dir/file");
    assert(strcmp(dirname(p), "dir") == 0);
    kfree(p);

    p = strdup("dir/file");
    assert(strcmp(pathname(p), "file") == 0);
    kfree(p);

    p = strdup("file");
    assert(strcmp(dirname(p), ".") == 0);
    kfree(p);

    p = strdup("file");
    assert(strcmp(pathname(p), "file") == 0);
    kfree(p);

    p = strdup("/");
    assert(strcmp(dirname(p), "/") == 0);
    kfree(p);

    p = strdup("/");
    assert(strcmp(pathname(p), "/") == 0);
    kfree(p);
}

