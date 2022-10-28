#ifndef _FRAMEWORK_H
#define _FRAMEWORK_H




/**
 * Four components in testing:
 * - the function under test (FUT), to verify its behavior
 * - a mock function, that will be injected to the FUT, to use preset mock values,
 * - a test case function, to setup the injection, set mock values, call the FUT, assert results,
 * - a function that collects all the test cases and asks the framework to run them.
 * 
 * int lookup(char *name, func_ptr disk_reader) {
 *     if (int err = (disk_reader)()) != 0)
 *         return err;
 *     ....
 * }
 * int emulate_disk_read(int sector, char *buffer, int len) {
 *     return mocked_value();
 * }
 * int test_lookup() {
 *     mock_value(32);
 *     int err = lookup("test", emulate_disk_read);
 *     assert(err == 32);
 * }
 * int main() {
 *     unit_test_t tests[] = {
 *         unit_test(test_lookup),
 *     };
 *     run_tests(tests);
 * }
 */

typedef void (*func_ptr)();
typedef struct unit_test unit_test_t;

#define FUNC_TYPE_TEST      1
#define FUNC_TYPE_SETUP     2
#define FUNC_TYPE_TEARDOWN  3

typedef struct unit_test {
    func_ptr func;
    char *func_name;
    int type; // see FUNC_TYPE_* defines
} unit_test_t;



// used to run a suite of tests
#define run_tests(tests)   \
            testing_framework_run_test_suite(tests, sizeof(tests)/sizeof(tests[0]))

// used to define an entry in the suite of tests
#define unit_test(test_func)  \
            {test_func, #test_func, FUNC_TYPE_TEST}

// used to define an entry in the suite of tests
#define unit_test_setup(test_func, setup_func)  \
            {setup_func, #test_func "_" #setup_func, FUNC_TYPE_SETUP}

// used to define an entry in the suite of tests
#define unit_test_teardown(test_func, teardown_func)  \
            {teardown_func, #test_func "_" #teardown_func, FUNC_TYPE_TEARDOWN}

// used to define an entry in the suite of tests
#define unit_test_with_setup_tear_down(test_func, setup_func, teardown_func)  \
            unit_test_setup(test_func, test_setup), \
            unit_test(test_func), \
            unit_test_teardown(test_func, test_teardown)

// used in the test case function, to test assertions
#define assert(x)   \
            testing_framework_assert((int)(x), #x, __FILE__, __LINE__)

// used in the test case function, to test assertions
#define fail(msg)   \
            testing_framework_fail("fail() called in test", msg, __FILE__, __LINE__)

// used in the test case function, to set mocked values for mock functions
#define mock_value(mock_func_ptr, value)  \
            testing_framework_add_mock_value(#mock_func_ptr, value)

// used in the mock function, to retrieve a mocked value
#define mocked_value()  \
            testing_framework_pop_mock_value(__FUNCTION__, __FILE__, __LINE__)

// used in the test case function, to set expectation of argument of the mock function
#define expect_string(mock_func_ptr, argument, str_ptr)  \
            testing_framework_register_expected_string(#mock_func_ptr "_" #argument, str_ptr)

// used in the test case function, to set expectation of argument of the mock function
#define expect_value(mock_func_ptr, argument, value)  \
            testing_framework_register_expected_value(#mock_func_ptr "_" #argument, value)

// used in the mock function, to perform validation of argument against expectation
#define check_expected(argument)  \
            testing_framework_check_mock_argument(__FUNCTION__ "_" #argument, (int)argument)


// used to forcibly fail a test
void testing_framework_fail(char *message, const char *data, const char *file, int line);

// used in the test case function, to test assertions
void testing_framework_assert(int result, char *expression, const char *file, int line);

// used in the test case function, to set mocked values for mock functions
void testing_framework_add_mock_value(const char *func_name, int value);

// used in the mock function, to retrieve a mocked value
int testing_framework_pop_mock_value(const char *func_name, const char *file, int line);

// used in the test case function, to set expectation of argument of the mock function
void testing_framework_register_expected_string(char *func_arg_name, char *value);

// used in the test case function, to set expectation of argument of the mock function
void testing_framework_register_expected_value(char *func_arg_name, int value);

// used in the mock function, to perform validation of argument against expectation
void testing_framework_check_mock_argument(char *func_arg_name, int value);

// used to run a suite of tests
bool testing_framework_run_test_suite(unit_test_t *tests, int count);


#endif