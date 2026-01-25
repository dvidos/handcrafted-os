#ifndef _TEST_CASE_H
#define _TEST_CASE_H

#include <stdint.h>
#include <stdbool.h>
#include "list.h"
#include "mock_func.h"


struct test_case_ops;

typedef struct test_case {
    // pointer into the suite unit test entry
    unit_test_t *unit_test;
    
    // key: injected function name, value: mock_function_info
    list_node_t *mock_funcs_list;
   
    // to detect memory leaks
    uint32_t free_heap_before;

    // retults from test execution
    bool failed;
    char *fail_message;
    char *fail_data;
    char *fail_file;
    int  fail_line;

    struct test_case_ops *ops;
} test_case_t;

struct test_case_ops {
    mock_func_info_t *(*get_mock_func_info)(test_case_t *test_case, const char *func_name, bool create_allowed, const char *file, int line);
    void (*log_debug_info)(test_case_t *test_case);
    void (*free_mock_func_info)(test_case_t *test_case);
    void (*free)(test_case_t *test_case);
};

test_case_t *create_test_case(unit_test_t *unit_test);


#endif
