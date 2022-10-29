#include <ctypes.h>
#include <string.h>
#include <drivers/screen.h>
#include <memory/kheap.h>
#include "framework.h"
#include <klog.h>

MODULE("UNITTEST");


static void log_debug_any_value(char *prefix, const void *value) {
    #define prn(c)    (((c)>=32 && (c)<=127) ? (c) : '.')
    klog_debug("%s int=%d, unsigned=%u, hex=0x%x, str=\"%c%c%c%c... (len=%d), bytes=%02x %02x %02x %02x...", 
        prefix,
        (int)value,
        (uint32_t)value,
        (uint32_t)value,
        prn(((char *)value)[0]),
        prn(((char *)value)[1]),
        prn(((char *)value)[2]),
        prn(((char *)value)[3]),
        strlen((char *)value),
        ((char *)value)[0],
        ((char *)value)[1],
        ((char *)value)[2],
        ((char *)value)[3]
    );
    #undef prn
}

// --------------------------------------------------------------------------

typedef void (*func_ptr)();
typedef void (*actor)(const void *payload, const void *extra_data);     
typedef int (*comparator)(const void *payload1, const void *payload2); // return 0 if equal

struct list_node;
struct list_node_ops;

typedef struct list_node {
    char *key;
    void *payload;
    struct list_node *next;
    struct list_node *prev;
    struct list_node_ops *ops;
} list_node_t;

struct list_node_ops {
    bool (*empty)(list_node_t *head);
    void (*add)(list_node_t *head, void *key, void *payload);
    void (*append)(list_node_t *head, list_node_t *node);
    list_node_t *(*first)(list_node_t *head);
    list_node_t *(*find_key)(list_node_t *head, const char *key);
    list_node_t *(*find_value)(list_node_t *head, const void *value, comparator compare);
    list_node_t *(*extract)(list_node_t *head, list_node_t *node);
    void (*delete)(list_node_t *head, list_node_t *node, actor payload_cleaner, void *extra_data);
    void (*debug)(list_node_t *head);
    void (*free)(list_node_t *head, actor payload_cleaner, void *extra_data);
};

static list_node_t *create_list();
static bool list_empty(list_node_t *head);
static void list_add(list_node_t *head, void *key, void *payload);
static void list_append(list_node_t *head, list_node_t *node);
static list_node_t *list_first(list_node_t *head);
static list_node_t *list_find_key(list_node_t *head, const char *key);
static list_node_t *list_find_value(list_node_t *head, const void *value, comparator compare);
static list_node_t *list_extract(list_node_t *head, list_node_t *node);
static void list_delete(list_node_t *head, list_node_t *node, actor payload_cleaner, void *extra_data);
static void list_debug(list_node_t *head);
static void list_free(list_node_t *head, actor payload_cleaner, void *extra_data);

struct list_node_ops list_node_ops = {
    .empty = list_empty,
    .add = list_add,
    .append = list_append,
    .first = list_first,
    .find_key = list_find_key,
    .find_value = list_find_value,
    .extract = list_extract,
    .delete = list_delete,
    .debug = list_debug,
    .free = list_free,
};

#define for_list(ptr, list)    for (list_node_t *(ptr) = list->next; ptr != list; ptr = ptr->next)

static list_node_t *create_list() {
    list_node_t *head = kmalloc(sizeof(list_node_t));
    head->next = head;
    head->prev = head;
    head->payload = NULL;
    head->ops = &list_node_ops;
    return head;
}

static void list_add(list_node_t *head, void *key, void *payload) {
    assert(head != NULL);
    list_node_t *node = kmalloc(sizeof(list_node_t));
    node->key = key;
    node->payload = payload;
    list_append(head, node);
}

static bool list_empty(list_node_t *head) {
    assert(head != NULL);
    return head->next == head;
}

static void list_append(list_node_t *head, list_node_t *node) {
    assert(head != NULL);
    assert(node != NULL);
    node->next = head;
    node->prev = head->prev;
    head->prev->next = node;
    head->prev = node;
}

static list_node_t *list_find_key(list_node_t *head, const char *key) {
    assert(head != NULL);
    list_node_t *node;
    for (node = head->next; node != head; node = node->next) {
        if (strcmp(node->key, key) == 0)
            return node;
    }
    return NULL;
}

static list_node_t *list_find_value(list_node_t *head, const void *value, comparator compare) {
    assert(head != NULL);
    assert(compare != NULL);
    list_node_t *node;
    for (node = head->next; node != head; node = node->next) {
        if (compare(node->payload, value) == 0)
            return node;
    }
    return NULL;
}

static list_node_t *list_first(list_node_t *head) {
    return head->next == head ? NULL : head->next;
}

static list_node_t *list_extract(list_node_t *head, list_node_t *node) {
    assert(head != NULL);
    assert(node != NULL);
    node->prev->next = node->next;
    node->next->prev = node->prev;
    return node;
}

static void list_delete(list_node_t *head, list_node_t *node, actor payload_cleaner, void *extra_data) {
    assert(head != NULL);
    assert(node != NULL);
    node->prev->next = node->next;
    node->next->prev = node->prev;
    if (payload_cleaner)
        payload_cleaner(node->payload, extra_data);
    kfree(node);
}

static void list_debug(list_node_t *head) {
    if (head == NULL) {
        klog_debug("head = NULL");
        return;
    }
    for_list (node, head) {
        klog_debug("- key  : %s", node->key);
        log_debug_any_value("  value: ", node->payload);
    }
}

static void list_free(list_node_t *head, actor payload_cleaner, void *extra_data) {
    while (!list_empty(head)) {
        list_delete(head, head->next, payload_cleaner, extra_data);
    }
    kfree(head);
}

// --------------------------------------------------------------------------

struct injected_func_ops;

typedef struct injected_func_info {
    // a non keyed list of mock values.
    // add with "mock_value()", get with "mocked_value()"
    list_node_t *mock_values_list;

    // a list of expected arguments.
    // key: function & argument name,
    // value: a list of arguments (to enable repeat calls to the injected method)
    list_node_t *expected_args_list;

    struct injected_func_ops *ops;
} injected_func_info_t;

struct injected_func_ops {
    void (*append_mock_value)(injected_func_info_t *info, int value);
    void (*append_expected_arg)(injected_func_info_t *info, char *arg_name, int expected_value);
    int (*dequeue_mock_value)(injected_func_info_t *info, const char *file, int line);
    int (*dequeue_expected_arg)(injected_func_info_t *info, char *arg_name, const char *file, int line);
    bool (*all_mock_values_used)(injected_func_info_t *info);
    char *(*get_unchecked_arg_name)(injected_func_info_t *info);
    void (*log_debug_info)(injected_func_info_t *info);
    void (*free)(injected_func_info_t *info);
};

static void injected_func_append_mock_value(injected_func_info_t *info, int value);
static void injected_func_append_expected_arg(injected_func_info_t *info, char *arg_name, int expected_value);
static int injected_func_dequeue_mock_value(injected_func_info_t *info, const char *file, int line);
static int injected_func_dequeue_expected_arg(injected_func_info_t *info, char *arg_name, const char *file, int line);
static bool injected_func_all_mock_values_used(injected_func_info_t *info);
static char *injected_func_get_unchecked_arg_name(injected_func_info_t *info);
static void injected_func_log_debug_info(injected_func_info_t *info);
static void injected_func_info_free(injected_func_info_t *info);

struct injected_func_ops injected_func_ops = {
    .append_mock_value = injected_func_append_mock_value,
    .append_expected_arg = injected_func_append_expected_arg,
    .dequeue_mock_value = injected_func_dequeue_mock_value,
    .dequeue_expected_arg = injected_func_dequeue_expected_arg,
    .all_mock_values_used = injected_func_all_mock_values_used,
    .get_unchecked_arg_name = injected_func_get_unchecked_arg_name,
    .log_debug_info = injected_func_log_debug_info,
    .free = injected_func_info_free
};

static injected_func_info_t *create_injected_func_info() {
    injected_func_info_t *info = kmalloc(sizeof(injected_func_info_t));
    info->mock_values_list = create_list();
    info->expected_args_list = create_list();
    info->ops = &injected_func_ops;
    return info;
}

static void injected_func_append_mock_value(injected_func_info_t *info, int value) {
    info->mock_values_list->ops->add(info->mock_values_list, NULL, (void *)value);
}
static void injected_func_append_expected_arg(injected_func_info_t *info, char *arg_name, int expected_value) {
    // each node has a list of expected values for this argument.
    list_node_t *arg_node = info->expected_args_list->ops->find_key(info->expected_args_list, arg_name);
    list_node_t *values_list;
    if (arg_node == NULL) {
        values_list = create_list();
        info->expected_args_list->ops->add(info->expected_args_list, arg_name, values_list);
    } else {
        values_list = (list_node_t *)arg_node->payload;
    }
    values_list->ops->add(values_list, NULL, (void *)expected_value);
}
static int injected_func_dequeue_mock_value(injected_func_info_t *info, const char *file, int line) {
    list_node_t *first = info->mock_values_list->ops->first(info->mock_values_list);
    if (first == NULL)
        testing_framework_test_failed("Mock value requested, but not setup", NULL, file, line);
    int value = (int)first->payload;
    info->mock_values_list->ops->delete(info->mock_values_list, first, NULL, NULL);
    return value;
}
static int injected_func_dequeue_expected_arg(injected_func_info_t *info, char *arg_name, const char *file, int line) {
    // each node has a list of expected values for this argument.
    list_node_t *arg_node = info->expected_args_list->ops->find_key(info->expected_args_list, arg_name);
    if (arg_node == NULL)
        testing_framework_test_failed("No arg list found for this argument", arg_name, file, line);
    
    list_node_t *values_list = (list_node_t *)arg_node->payload;
    list_node_t *first = values_list->ops->first(values_list);
    if (first == NULL)
        testing_framework_test_failed("Arg list for this argument is empty", arg_name, file, line);

    int value = (int)first->payload;
    values_list->ops->delete(values_list, first, NULL, NULL);
    return value;
}

static bool injected_func_all_mock_values_used(injected_func_info_t *info) {
    return info->mock_values_list->ops->empty(info->mock_values_list);
}

static char *injected_func_get_unchecked_arg_name(injected_func_info_t *info) {
    for_list(arg_node,  info->expected_args_list) {
        list_node_t *values_list = (list_node_t *)arg_node->payload;
        if (!values_list->ops->empty(values_list))
            return arg_node->key;
    }
    return NULL;
}

static void injected_func_log_debug_info(injected_func_info_t *info) {
    if (!info->mock_values_list->ops->empty(info->mock_values_list)) {
        klog_debug("      Mocked values:");
        for_list(node, info->mock_values_list) {
            log_debug_any_value("      - ", node->payload);
        }
    }

    if (!info->expected_args_list->ops->empty(info->expected_args_list)) {
        klog_debug("      Arguments validations:");
        for_list(arg_node,  info->expected_args_list) {
            klog_debug("      - \"%s\"", arg_node->key);
            list_node_t *values_list = (list_node_t *)arg_node->payload;
            for_list(val_node, values_list) {
                log_debug_any_value("        - ", val_node->payload);
            }
        }
    }
}
static void injected_func_info_free(injected_func_info_t *info) {
    info->mock_values_list->ops->free(info->mock_values_list, NULL, NULL);

    for_list (args_list, info->expected_args_list) {
        list_node_t *values_list = (list_node_t *)args_list->payload;
        values_list->ops->debug(values_list);
        values_list->ops->free(values_list, NULL, NULL);
    }

    info->expected_args_list->ops->free(info->expected_args_list, NULL, NULL);
    kfree(info);
}

// ------------------------------------------------------------------------

struct test_case_ops;

typedef struct test_case {
    // pointer into the suite unit test entry
    unit_test_t *unit_test;
    
    // key: injected function name, value: injected_function_info
    list_node_t *injected_funcs_list;
   
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
    injected_func_info_t *(*get_injected_func_info)(test_case_t *test_case, const char *func_name, bool create_allowed, const char *file, int line);
    void (*log_debug_info)(test_case_t *test_case);
    void (*free_injected_func_info)(test_case_t *test_case);
    void (*free)(test_case_t *test_case);
};

static test_case_t *create_test_case(unit_test_t *unit_test);
static injected_func_info_t *test_case_get_injected_func_info(test_case_t *test_case, const char *func_name, bool create_allowed, const char *file, int line);
static void test_case_log_debug_info(test_case_t *test_case);
static void test_case_free_injected_func_info(test_case_t *test_case);
static void test_case_free(test_case_t *test_case);


struct test_case_ops test_case_ops = {
    .get_injected_func_info = test_case_get_injected_func_info,
    .log_debug_info = test_case_log_debug_info,
    .free_injected_func_info = test_case_free_injected_func_info,
    .free = test_case_free,
};

static test_case_t *create_test_case(unit_test_t *unit_test) {
    test_case_t *tc = kmalloc(sizeof(test_case_t));
    memset(tc, 0, sizeof(test_case_t));
    tc->unit_test = unit_test;
    tc->injected_funcs_list = create_list();
    tc->ops = &test_case_ops;
    return tc;
}

static injected_func_info_t *test_case_get_injected_func_info(test_case_t *test_case, const char *func_name,
        bool create_allowed, const char *file, int line) {
    assert(test_case != NULL);
    assert(test_case->injected_funcs_list != NULL);
    list_node_t *func_node = test_case->injected_funcs_list->ops->find_key(test_case->injected_funcs_list, func_name);
    if (func_node != NULL) {
        return (injected_func_info_t *)func_node->payload;
    }
    if (create_allowed) {
        injected_func_info_t *func_info = create_injected_func_info();
        test_case->injected_funcs_list->ops->add(test_case->injected_funcs_list, (char *)func_name, func_info);
        return func_info;
    }
    testing_framework_test_failed("Injected func info not found", func_name, file, line);
    return NULL;
}

static void test_case_log_debug_info(test_case_t *test_case) {
    klog_debug("Debug info for test case \"%s\"", test_case->unit_test->func_name);
    if (test_case->failed) {
        klog_debug("  fail reason: %s %s", test_case->fail_message, test_case->fail_data);
        klog_debug("  at %s, line %d", test_case->fail_file, test_case->fail_line);
    }
    if (!test_case->injected_funcs_list->ops->empty(test_case->injected_funcs_list)) {
        klog_debug("  Injected functions mock information");
        for_list(node, test_case->injected_funcs_list) {
            klog_debug("    - %s()", node->key);
            injected_func_info_t *func_info = (injected_func_info_t *)node->payload;
            func_info->ops->log_debug_info(func_info);
        }
    }
}

static void test_case_free_injected_func_info(test_case_t *tc) {
    // clear allocations for mock values, but not the list, 
    // as it is created in create(), which is when we measure free heap.
    list_node_t *funcs_list = tc->injected_funcs_list;
    while (!funcs_list->ops->empty(funcs_list)) {
        list_node_t *node = funcs_list->ops->first(funcs_list);
        injected_func_info_t *fi = (injected_func_info_t *)node->payload;
        fi->ops->free(fi);
        funcs_list->ops->delete(funcs_list, node, NULL, NULL);
    }
}
static void test_case_free(test_case_t *tc) {
    test_case_free_injected_func_info(tc);
    list_free(tc->injected_funcs_list, NULL, NULL);
    kfree(tc);
}

// --------------------------------------------------------------------

static struct test_suite {
    int tests_performed;
    int tests_passed;
    int tests_failed;

    // to detect framework memory leaks
    uint32_t free_heap_before;

    // unkeyed list of test_case_t as payloads
    list_node_t *test_cases;

    // pointer to currently running test case
    test_case_t *curr_test_case;
} test_suite;

#define FUNC_TYPE_TEST      1
#define FUNC_TYPE_SETUP     2
#define FUNC_TYPE_TEARDOWN  3

void testing_framework_test_failed(const char *message, const char *data, const char *file, int line) {
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
void testing_framework_assert(int result, char *expression, const char *file, int line) {
    if (result) return;
    testing_framework_test_failed("assert() failed", expression, file, line);
}

// used in the test case function, to set mocked values for mock functions
void testing_framework_add_mock_value(const char *func_name, int value) {
    test_case_t *tc = test_suite.curr_test_case;
    injected_func_info_t *f = tc->ops->get_injected_func_info(tc, func_name, true, NULL, 0);
    f->ops->append_mock_value(f, value);
}

// used in the mock function, to retrieve a mocked value
int testing_framework_pop_mock_value(const char *func_name, const char *file, int line) {
    test_case_t *tc = test_suite.curr_test_case;
    injected_func_info_t *f = tc->ops->get_injected_func_info(tc, func_name, false, file, line);
    return f->ops->dequeue_mock_value(f, file, line);
}

// used in the test case function, to set expectation of argument of the injected function
void testing_framework_register_expected_value(char *func_name, char *arg_name, int value) {
    test_case_t *tc = test_suite.curr_test_case;
    injected_func_info_t *f = tc->ops->get_injected_func_info(tc, func_name, true, NULL, 0);
    f->ops->append_expected_arg(f, arg_name, value);
}

// used in the injected function, to perform validation of argument against expectation
void testing_framework_check_numeric_argument(char *func_name, char *arg_name, int value, const char *file, int line) {
    test_case_t *tc = test_suite.curr_test_case;
    injected_func_info_t *f = tc->ops->get_injected_func_info(tc, func_name, false, file, line);
    int expected = f->ops->dequeue_expected_arg(f, arg_name, file, line);
    if (value != expected)
        testing_framework_test_failed("Expected argument mismatch", arg_name, file, line);
}

void testing_framework_check_str_argument(const char *func_name, char *arg_name, char *value, const char *file, int line) {
    test_case_t *tc = test_suite.curr_test_case;
    injected_func_info_t *f = tc->ops->get_injected_func_info(tc, func_name, false, file, line);
    int expected = f->ops->dequeue_expected_arg(f, arg_name, file, line);
    if (strcmp(value, (char *)expected) != 0)
        testing_framework_test_failed("Expected argument mismatch", arg_name, file, line);
}

void testing_framework_debug_test_setup() {
    test_case_t *tc = test_suite.curr_test_case;
    tc->ops->log_debug_info(tc);
}

// ---------------------------------------------------------

static void before_running_tests_suite() {
    test_suite.free_heap_before = kernel_heap_free_size();
    test_suite.tests_performed = 0;
    test_suite.tests_passed = 0;
    test_suite.tests_failed = 0;
    test_suite.curr_test_case = NULL;
    test_suite.test_cases = create_list();
}

static void after_running_tests_suite() {
    // newline after lots of dots
    printk("\n");

    klog_info("%d tests executed, %d passed, %d failed", 
        test_suite.tests_performed,
        test_suite.tests_passed,
        test_suite.tests_failed
    );
    
    if (test_suite.tests_failed) {
        klog_info("Failed tests:");
        for_list (tc_node, test_suite.test_cases) {
            test_case_t *tc = tc_node->payload;
            if (!tc->failed)
                continue;
            
            klog_info("- %s()", tc->unit_test->func_name);
            klog_info("  %s: %s, at %s:%d", tc->fail_message, tc->fail_data, tc->fail_file, tc->fail_line);
        }
    }

    // after reporting, we can clean up the test cases
    for_list(tc_node, test_suite.test_cases) {
        test_case_t *tc = tc_node->payload;
        tc->ops->free(tc);
    }
    list_free(test_suite.test_cases, NULL, NULL);

    if (kernel_heap_free_size() != test_suite.free_heap_before) {
        klog_error("Suspecting testing framework leaking memory of %u bytes", 
            test_suite.free_heap_before - kernel_heap_free_size());
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
    for_list(fi_node, tc->injected_funcs_list) {
        injected_func_info_t *fi = fi_node->payload;
        if (!fi->ops->all_mock_values_used(fi)) {
            fail("Not all mock values were used in injected function", fi_node->key);
        }

        if (fi->ops->get_unchecked_arg_name(fi) != NULL) {
            fail("Not all arguments were chhecked in injected function", fi_node->key);
        }
    }

    // now we can clean this info up and verify heap
    tc->ops->free_injected_func_info(tc);
    if (kernel_heap_free_size() != tc->free_heap_before)
        fail("Test case memory leak suspected!", tc->unit_test->func_name);

    // we are not cleaning up the test case here, to allow for reporting (and investigating) later.
    test_suite.tests_performed++;
    if (tc->failed) {
        test_suite.tests_failed++;
        printk("F");
    } else {
        test_suite.tests_passed++;
        printk(".");
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


bool testing_framework_run_test_suite(unit_test_t tests[], int num_tests) {

    before_running_tests_suite();
    for (int i = 0; i < num_tests; i++) {
        run_one_test(&tests[i]);
    }
    bool all_succeeded = test_suite.tests_failed == 0;
    after_running_tests_suite();

    return all_succeeded;
}

