#ifndef _TEST_SUITE_H
#define _TEST_SUITE_H

#include <stdint.h>
#include "list.h"
#include "test_case.h"


struct test_suite {
    int tests_performed;
    int tests_passed;
    int tests_failed;

    // to detect framework memory leaks
    uint32_t free_heap_before;

    // unkeyed list of test_case_t as payloads
    list_node_t *test_cases;

    // pointer to currently running test case
    test_case_t *curr_test_case;
};

#define FUNC_TYPE_TEST      1
#define FUNC_TYPE_SETUP     2
#define FUNC_TYPE_TEARDOWN  3


bool run_test_suite(unit_test_t tests[], int num_tests);

// how about results?

#endif
