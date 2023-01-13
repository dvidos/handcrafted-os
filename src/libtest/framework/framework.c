#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../include/testing.h"
#include "setup.h"
#include "list.h"
#include "mock_func.h"
#include "test_case.h"
#include "test_suite.h"



// variable where all housekeeping is stored
// the suite, information for each test succeeding or failing, reasons, etc.
static struct test_suite test_suite;



void _testing_framework_test_failed(const char *message, const char *data, const char *file, int line) {
    test_case_t *tc = test_suite.curr_test_case;
    if (tc != NULL) {
        tc->failed = true;
        tc->fail_message = (char *)message;
        tc->fail_data = (char *)data;
        tc->fail_file = (char *)file;
        tc->fail_line = line;
    }
}

// used in the test case function, to test assertions
void _testing_framework_assert(int result, char *expression, const char *file, int line) {
    if (result) return;
    _testing_framework_test_failed("assert() failed", expression, file, line);
}

// used in the test case function, to set mocked values for mock functions
void _testing_framework_add_mock_value(const char *func_name, scalar value) {
    test_case_t *tc = test_suite.curr_test_case;
    mock_func_info_t *f = tc->ops->get_mock_func_info(tc, func_name, true, NULL, 0);
    f->ops->append_mock_value(f, value);
}

// used in the mock function, to retrieve a mocked value
scalar _testing_framework_pop_mock_value(const char *func_name, const char *file, int line) {
    test_case_t *tc = test_suite.curr_test_case;
    mock_func_info_t *f = tc->ops->get_mock_func_info(tc, func_name, false, file, line);
    return f->ops->dequeue_mock_value(f, file, line);
}

// used in the test case function, to set expectation of argument of the injected function
void _testing_framework_register_expected_value(char *func_name, char *arg_name, scalar value) {
    test_case_t *tc = test_suite.curr_test_case;
    mock_func_info_t *f = tc->ops->get_mock_func_info(tc, func_name, true, NULL, 0);
    f->ops->append_expected_arg(f, arg_name, value);
}

// used in the injected function, to perform validation of argument against expectation
void _testing_framework_check_numeric_argument(char *func_name, char *arg_name, scalar value, const char *file, int line) {
    test_case_t *tc = test_suite.curr_test_case;
    mock_func_info_t *f = tc->ops->get_mock_func_info(tc, func_name, false, file, line);
    scalar expected = f->ops->dequeue_expected_arg(f, arg_name, file, line);
    if (value != expected)
        _testing_framework_test_failed("Expected argument mismatch", arg_name, file, line);
}

void _testing_framework_check_str_argument(const char *func_name, char *arg_name, char *value, const char *file, int line) {
    test_case_t *tc = test_suite.curr_test_case;
    mock_func_info_t *f = tc->ops->get_mock_func_info(tc, func_name, false, file, line);
    scalar expected = f->ops->dequeue_expected_arg(f, arg_name, file, line);
    if (strcmp(value, (char *)expected) != 0)
        _testing_framework_test_failed("Expected argument mismatch", arg_name, file, line);
}


