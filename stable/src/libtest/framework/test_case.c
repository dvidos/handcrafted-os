#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "setup.h"
#include "../include/testing.h"
#include "test_case.h"
#include "list.h"

static mock_func_info_t *test_case_get_mock_func_info(test_case_t *test_case, const char *func_name, bool create_allowed, const char *file, int line);
static void test_case_log_debug_info(test_case_t *test_case);
static void test_case_free_mock_func_info(test_case_t *test_case);
static void test_case_free(test_case_t *test_case);

struct test_case_ops test_case_ops = {
    .get_mock_func_info = test_case_get_mock_func_info,
    .log_debug_info = test_case_log_debug_info,
    .free_mock_func_info = test_case_free_mock_func_info,
    .free = test_case_free,
};

test_case_t *create_test_case(unit_test_t *unit_test) {
    test_case_t *tc = testing_framework_setup.malloc(sizeof(test_case_t));
    memset(tc, 0, sizeof(test_case_t));
    tc->unit_test = unit_test;
    tc->mock_funcs_list = create_list();
    tc->ops = &test_case_ops;
    return tc;
}

static mock_func_info_t *test_case_get_mock_func_info(test_case_t *test_case, const char *func_name,
        bool create_allowed, const char *file, int line) {
    assert(test_case != NULL);
    assert(test_case->mock_funcs_list != NULL);
    list_node_t *func_node = test_case->mock_funcs_list->ops->find_key(test_case->mock_funcs_list, func_name);
    if (func_node != NULL) {
        return (mock_func_info_t *)func_node->payload;
    }
    if (create_allowed) {
        mock_func_info_t *func_info = create_mock_func_info();
        test_case->mock_funcs_list->ops->add(test_case->mock_funcs_list, (char *)func_name, func_info);
        return func_info;
    }
    _testing_framework_test_failed("Injected func info not found", func_name, file, line);
    return NULL;
}

static void test_case_log_debug_info(test_case_t *test_case) {
    testing_framework_setup.printf("Debug info for test case \"%s\"", test_case->unit_test->func_name);
    if (test_case->failed) {
        testing_framework_setup.printf("  fail reason: %s %s", test_case->fail_message, test_case->fail_data);
        testing_framework_setup.printf("  at %s, line %d", test_case->fail_file, test_case->fail_line);
    }
    if (!test_case->mock_funcs_list->ops->empty(test_case->mock_funcs_list)) {
        testing_framework_setup.printf("  Injected functions mock information");
        for_list(node, test_case->mock_funcs_list) {
            testing_framework_setup.printf("    - %s()", node->key);
            mock_func_info_t *func_info = (mock_func_info_t *)node->payload;
            func_info->ops->log_debug_info(func_info);
        }
    }
}

static void test_case_free_mock_func_info(test_case_t *tc) {
    // clear allocations for mock values, but not the list, 
    // as it is created in create(), which is when we measure free heap.
    list_node_t *funcs_list = tc->mock_funcs_list;
    while (!funcs_list->ops->empty(funcs_list)) {
        list_node_t *node = funcs_list->ops->first(funcs_list);
        mock_func_info_t *fi = (mock_func_info_t *)node->payload;
        fi->ops->free(fi);
        funcs_list->ops->remove(funcs_list, node, NULL, NULL);
    }
}

static void test_case_free(test_case_t *tc) {
    tc->ops->free_mock_func_info(tc);
    tc->mock_funcs_list->ops->free(tc->mock_funcs_list, NULL, NULL);
    testing_framework_setup.free(tc);
}

