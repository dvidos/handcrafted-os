#include <stdint.h>
#include <stddef.h>
#include "../include/testing.h"
#include "setup.h"
#include "list.h"
#include "test_case.h"
#include "test_suite.h"


static struct test_suite test_suite;


static void before_running_test_suite() {
    if (testing_framework_setup.heap_free != NULL)
        test_suite.free_heap_before = testing_framework_setup.heap_free();
    test_suite.tests_performed = 0;
    test_suite.tests_passed = 0;
    test_suite.tests_failed = 0;
    test_suite.curr_test_case = NULL;
    test_suite.test_cases = create_list();
}

static void after_running_test_suite() {
    // newline after lots of dots
    testing_framework_setup.printf("\n");

    testing_framework_setup.printf("%d tests executed, %d passed, %d failed", 
        test_suite.tests_performed,
        test_suite.tests_passed,
        test_suite.tests_failed
    );
    
    if (test_suite.tests_failed) {
        testing_framework_setup.printf("Failed tests:");
        for_list (tc_node, test_suite.test_cases) {
            test_case_t *tc = tc_node->payload;
            if (!tc->failed)
                continue;
            
            testing_framework_setup.printf("- %s()", tc->unit_test->func_name);
            testing_framework_setup.printf("  %s: %s, at %s:%d", tc->fail_message, tc->fail_data, tc->fail_file, tc->fail_line);
        }
    }

    // after reporting, we can clean up the test cases
    for_list(tc_node, test_suite.test_cases) {
        test_case_t *tc = tc_node->payload;
        tc->ops->free(tc);
    }
    test_suite.test_cases->ops->free(test_suite.test_cases, NULL, NULL);

    if (testing_framework_setup.heap_free != NULL) {
        uint32_t heap_free_after = testing_framework_setup.heap_free();
        if (heap_free_after != test_suite.free_heap_before) {
            testing_framework_setup.printf("Suspecting testing framework leaking memory of %u bytes", 
                test_suite.free_heap_before - heap_free_after);
        }
    }
}

static void before_running_test_case(test_case_t *tc) {
    test_suite.test_cases->ops->add(test_suite.test_cases, NULL, tc);
    test_suite.curr_test_case = tc;

    // this measurement after creating the test_case, but before any mock values
    tc->free_heap_before = kernel_heap_free_size();
}

static void after_running_test_case(test_case_t *tc) {

    // lets make sure all setup was utilized
    for_list(fi_node, tc->mock_funcs_list) {
        mock_func_info_t *fi = fi_node->payload;
        if (!fi->ops->all_mock_values_used(fi)) {
            fail("Not all mock values were used in injected function", fi_node->key);
        }

        if (fi->ops->get_unchecked_arg_name(fi) != NULL) {
            fail("Not all arguments were chhecked in injected function", fi_node->key);
        }
    }

    // now we can clean this info up and verify heap
    tc->ops->free_mock_func_info(tc);
    if (kernel_heap_free_size() != tc->free_heap_before)
        fail("Test case memory leak suspected!", tc->unit_test->func_name);

    // we are not cleaning up the test case here, to allow for reporting (and investigating) later.
    test_suite.tests_performed++;
    if (tc->failed) {
        test_suite.tests_failed++;
        testing_framework_setup.printf("F");
    } else {
        test_suite.tests_passed++;
        testing_framework_setup.printf(".");
    }
    // do this last
    test_suite.curr_test_case = NULL;
}

static void run_one_test(unit_test_t *test) {
    // test cases are created & added as they occur
    test_case_t *test_case = create_test_case(test);

    before_running_test_case(test_case);
    test->func();
    after_running_test_case(test_case);
}


bool run_test_suite(unit_test_t tests[], int num_tests) {

    before_running_test_suite();
    for (int i = 0; i < num_tests; i++) {
        run_one_test(&tests[i]);
    }
    bool all_succeeded = test_suite.tests_failed == 0;
    after_running_test_suite();

    return all_succeeded;
}

