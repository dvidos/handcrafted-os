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

int mock_filesys_create(char *name) {
    // check_expected(name);
    return mocked_value();
}

// ---- test cases code ----

// test cases
void test_null_name_returns_bad_argument() {
    int err = create_file(NULL);
    assert(err == ERR_BAD_ARGUMENT);
}

void test_empty_name_returns_bad_argument() {
    int err = create_file("");
    assert(err == ERR_BAD_VALUE);
}

void test_create_happy_path() {
    expect_string(mock_filesys_create, name, "testing");
    mock_value(mock_filesys_create, SUCCESS);
    filesys_create_ptr = mock_filesys_create;

    int err = create_file("testing");
    assert(err == SUCCESS);
}

void test_create_lower_error_is_returned() {
    expect_string(mock_filesys_create, name, "testing");
    mock_value(mock_filesys_create, ERR_NOT_SUPPORTED);
    filesys_create_ptr = mock_filesys_create;

    int err = create_file("testing");
    assert(err == ERR_NOT_SUPPORTED);
}

void test_something_really_peculiar() {
    assert(1==2);
    assert(3==3);
}

void noop() {
    klog_debug("Hello from NOOP test!");
}

void noop2() {
    klog_debug("Hello from NOOP II test!");
    //char *p = kmalloc(100);
}

// final code to be run
bool run_frameworked_unit_tests_demo() {
    unit_test_t tests[] = {
        unit_test(noop),
        unit_test(noop2),
        // unit_test(test_create_happy_path),
        // unit_test(test_create_lower_error_is_returned),
        // unit_test(test_null_name_returns_bad_argument),
        // unit_test(test_empty_name_returns_bad_argument),
        unit_test(test_something_really_peculiar),
    };
    return run_tests(tests);
}
