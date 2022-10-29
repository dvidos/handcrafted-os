#include <ctypes.h>
#include <errors.h>
#include <string.h>
#include <drivers/screen.h>
#include "framework.h"
#include <klog.h>
#include <memory/kheap.h>

MODULE("UNITTEST");

// ---- actual code under test -----

int (*filesys_create_ptr)(char *name);

int create_file(char *name) {
    if (name == NULL || strlen(name) == 0)
        return ERR_BAD_ARGUMENT;
    return filesys_create_ptr(name);
}

static int filesys_create_injected_func(char *name) {
    check_str_arg(name);
    return mocked_value();
}

// ---- test cases code ----

// test cases
void test_null_name_returns_bad_argument() {
    filesys_create_ptr = filesys_create_injected_func;
    int err = create_file(NULL);
    assert(err == ERR_BAD_ARGUMENT);
}

void test_empty_name_returns_bad_argument() {
    debug_test_setup();
    filesys_create_ptr = filesys_create_injected_func;
    int err = create_file("");
    assert(err == ERR_BAD_ARGUMENT);
}

void test_create_happy_path() {
    expect_arg(filesys_create_injected_func, name, "testing");
    mock_value(filesys_create_injected_func, SUCCESS);
    filesys_create_ptr = filesys_create_injected_func;

    // before running, just to see
    debug_test_setup();

    int err = create_file("testing");
    assert(err == SUCCESS);
}

void test_create_lower_error_is_returned() {
    expect_arg(filesys_create_injected_func, name, "testing");
    mock_value(filesys_create_injected_func, ERR_NOT_SUPPORTED);

    filesys_create_ptr = filesys_create_injected_func;
    int err = create_file("testing");
    assert(err == ERR_NOT_SUPPORTED);
}

// final code to be run
bool run_frameworked_unit_tests_demo() {
    unit_test_t tests[] = {
        unit_test(test_create_happy_path),
        unit_test(test_create_lower_error_is_returned),
        unit_test(test_null_name_returns_bad_argument),
        unit_test(test_empty_name_returns_bad_argument),
    };
    return run_tests(tests);
}
