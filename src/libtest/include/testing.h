#ifndef _TESTING_H
#define _TESTING_H


// ----- framework functionality ------------------------------

// initialize the framework
void testing_framework_init(
    void *(*memory_allocator)(unsigned size),
    void (*memory_deallocator)(void *address),
    void (*printf_function)(char *format, ...),
    unsigned long (*heap_free_reporter)()
);

void testing_framework_debug_setup();



// ----- test suite macros & functions -----------------------


#define FUNC_TYPE_TEST      1
#define FUNC_TYPE_SETUP     2
#define FUNC_TYPE_TEARDOWN  3

typedef struct unit_test {
    void (*func)();
    char *func_name;
    int type; // see FUNC_TYPE_* defines
} unit_test_t;



// used to run a suite of tests
#define run_tests(tests)   \
            _testing_framework_run_test_suite(tests, sizeof(tests)/sizeof(tests[0]))

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



// ----- test cases setup macros & functions ---------------- 


// used in the test case function, to set mocked values for mock functions
#define mock_value(mock_func_ptr, value)  \
            _testing_framework_add_mock_value(#mock_func_ptr, value)

// used in the test case function, to set expectation of argument of the mock function
#define expect_arg(mock_func_ptr, argument, value)  \
            _testing_framework_register_expected_value(#mock_func_ptr, #argument, (int)(value))

// used in the test case function, to test assertions
#define assert(x)   \
            _testing_framework_assert((int)(x), #x, __FILE__, __LINE__)

// used in the test case function, to test assertions
#define fail(msg, data)   \
            _testing_framework_test_failed(msg, data, __FILE__, __LINE__)


void _testing_framework_add_mock_value(const char *func_name, int value);
void _testing_framework_register_expected_value(char *func_name, char *arg_name, int value);
void _testing_framework_assert(int result, char *expression, const char *file, int line);
void _testing_framework_test_failed(const char *message, const char *data, const char *file, int line);


// ------ mock functions macros & functions ------------------


// used in the mock function, to retrieve a mocked value
#define mocked_value()  \
            _testing_framework_pop_mock_value(__FUNCTION__, __FILE__, __LINE__)

// used in the mock function, to perform validation of argument against expectation
#define check_num_arg(argument)  \
            _testing_framework_check_numeric_argument(__FUNCTION__, #argument, (int)argument, __FILE__, __LINE__)

// used in the mock function, to perform validation of argument against expectation
#define check_str_arg(argument)  \
            _testing_framework_check_str_argument(__FUNCTION__, #argument, (char *)argument, __FILE__, __LINE__)


int _testing_framework_pop_mock_value(const char *func_name, const char *file, int line);
void _testing_framework_check_numeric_argument(char *func_name, char *arg_name, int value, const char *file, int line);
void _testing_framework_check_str_argument(const char *func_name, char *arg_name, char *value, const char *file, int line);




#endif // _TESTING_H
