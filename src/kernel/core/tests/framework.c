#include <ctypes.h>
#include <string.h>
#include <drivers/screen.h>
#include <memory/kheap.h>
#include "framework.h"
#include <klog.h>

MODULE("UNITTEST");


typedef void (*func_ptr)();
typedef void (*actor)(const void *payload, const void *extra_data);     
typedef int (*comparator)(const void *payload1, const void *payload2); // return 0 if equal

struct list_node;
struct list_node_ops;

typedef struct list_node {
    struct list_node *next;
    struct list_node *prev;
    const void *key;
    const void *payload;
} list_node_t;


list_node_t *create_list();
void list_append(list_node_t *head, list_node_t *node);
void list_add(list_node_t *head, const void *key, const void *payload);
list_node_t *list_extract(list_node_t *head, list_node_t *node);
void list_delete(list_node_t *head, list_node_t *node, actor cleaner, void *extra_data);
bool list_empty(list_node_t *head);
void destroy_list(list_node_t *head, actor cleaner, void *extra_data);
list_node_t *list_find_key(list_node_t *head, const char *key);
list_node_t *list_find(list_node_t *head, const void *value, comparator compare);


#define for_list(list, ptr)    for (list_node_t *(ptr) = list->next; ptr != list; ptr = ptr->next)

list_node_t *create_list() {
    list_node_t *head = kmalloc(sizeof(list_node_t));
    head->next = head;
    head->prev = head;
    head->payload = NULL;
    return head;
}

void list_append(list_node_t *head, list_node_t *node) {
    assert(head != NULL);
    assert(node != NULL);
    node->next = head;
    node->prev = head->prev;
    head->prev->next = node;
    head->prev = node;
}

void list_add(list_node_t *head, const void *key, const void *payload) {
    assert(head != NULL);
    list_node_t *node = kmalloc(sizeof(list_node_t));
    node->key = key;
    node->payload = payload;
    list_append(head, node);
}

list_node_t *list_extract(list_node_t *head, list_node_t *node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    return node;
}

void list_delete(list_node_t *head, list_node_t *node, actor cleaner, void *extra_data) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    if (cleaner)
        cleaner(node->payload, extra_data);
    kfree(node);
}

bool list_empty(list_node_t *head) {
    return head->next == head;
}

void destroy_list(list_node_t *head, actor cleaner, void *extra_data) {
    while (!list_empty(head)) {
        list_delete(head, head->next, cleaner, extra_data);
    }
    kfree(head);
}

list_node_t *list_find_key(list_node_t *head, const char *key) {
    list_node_t *node;
    for (node = head->next; node != head; node = node->next) {
        if (strcmp(node->key, key) == 0)
            return node;
    }
    return NULL;
}

list_node_t *list_find(list_node_t *head, const void *value, comparator compare) {
    list_node_t *node;
    for (node = head->next; node != head; node = node->next) {
        if (compare(node->payload, value) == 0)
            return node;
    }
    return NULL;
}

// --------------------------------------------------------------------------


// --------------------------------------------------------------------------



struct func_info {
    char *name;
    list_node_t *mock_values_queue;
};

struct test_info {
    unit_test_t *unit_test;
    char *name;
    bool passed;
    char *fail_message;
    char *fail_data;
    char *fail_file;
    int fail_line;
};

struct testing_framework_data {
    int tests_performed;
    int tests_passed;
    int tests_failed;
    struct test_info *current_test_info;

    list_node_t *info_per_func;
    list_node_t *info_per_test;
} testing_framework_data;


#define FUNC_TYPE_TEST      1
#define FUNC_TYPE_SETUP     2
#define FUNC_TYPE_TEARDOWN  3


void testing_framework_fail(char *message, const char *data, const char *file, int line) {
    // mark current test as failed, grab failure information.
    struct test_info *ti = testing_framework_data.current_test_info;
    if (ti != NULL) {
        ti->passed = false;
        ti->fail_message = (char *)message;
        ti->fail_data = (char *)data;
        ti->fail_file = (char *)file;
        ti->fail_line = line;
    }
}

// used in the test case function, to test assertions
void testing_framework_assert(int result, char *expression, const char *file, int line) {
    if (result)
        return;

    testing_framework_fail("Assertion failed", expression, file, line);
}

// used in the test case function, to set mocked values for mock functions
void testing_framework_add_mock_value(const char *func_name, int value) {

    list_node_t *func_node = list_find_key(testing_framework_data.info_per_func, func_name);
    list_node_t *mock_values_list;
    if (func_node == NULL) {
        mock_values_list = (list_node_t *)func_node->payload;
    } else {
        mock_values_list = create_list();
        list_add(testing_framework_data.info_per_func, func_name, mock_values_list);
    }

    list_add(mock_values_list, NULL, (void *)value);
}

// used in the mock function, to retrieve a mocked value
int testing_framework_pop_mock_value(const char *func_name, const char *file, int line) {

    list_node_t *func_node = list_find_key(testing_framework_data.info_per_func, func_name);
    if (func_node == NULL) {
        testing_framework_fail("No function info found for function", func_name, __FILE__, __LINE__);
        return 0;
    }
    list_node_t *mock_values_list = (list_node_t *)func_node->payload;
    if (mock_values_list == NULL) {
        testing_framework_fail("No mocked values list found for function", func_name, __FILE__, __LINE__);
        return 0;
    }
    if (list_empty(mock_values_list)) {
        testing_framework_fail("Mocked values list is empty for function", func_name, __FILE__, __LINE__);
        return 0;
    }
    // pop the first one
    list_node_t *first_node = list_extract(mock_values_list, mock_values_list->next);
    int ret_val = (int)first_node->payload;
    kfree(first_node);
    
    return ret_val;
}

// used in the test case function, to set expectation of argument of the mock function
void testing_framework_register_expected_string(char *func_arg_name, char *value) {
    // 
}

// used in the test case function, to set expectation of argument of the mock function
void testing_framework_register_expected_value(char *func_arg_name, int value) {

}

// used in the mock function, to perform validation of argument against expectation
void testing_framework_check_mock_argument(char *func_arg_name, int value) {

}

void debug_testing_framework_data() {
    #define prn(c)    (((c)>=32 && (c)<=127) ? (c) : '.')

    klog_debug("Testing Framework Data Follows");
    klog_debug("  %d tests performed, %d passed, %d failed",
        testing_framework_data.tests_performed,
        testing_framework_data.tests_passed,
        testing_framework_data.tests_failed
    );
    klog_debug("  Mocking info:");
    int i = 1;
    for_list(testing_framework_data.info_per_func, p) {
        klog_debug("    mock values for function: %s()", p->key);
        list_node_t *values_list = (list_node_t *)p->payload;
        for_list(values_list, v) {
            char *s = (char *)v->payload;
            klog_debug("    %2s int %d, uint %u, hex 0x%x, s %c%c%c%c..., binary %02x %02x %02x %02x...",
                i++,
                (int)s, (uint32_t)s, (uint32_t)s, 
                prn(s[0]), prn(s[1]), prn(s[2]), prn(s[3]), 
                s[0], s[1], s[2], s[3]
            );
        }
    }

    klog_debug("  Test function info:");
    for_list(testing_framework_data.info_per_test, p) {
        struct test_info *ti = (struct test_info *)p->payload;
        klog_debug("  - %s(), %s", ti->name, ti->passed ? "passed" : "FAILED");
        if (!ti->passed)
            klog_debug("    %s: %s (%s, line %d)", ti->fail_message, ti->fail_data, ti->fail_file, ti->fail_line);
    }

    #undef prn
}
// ---------------------------------------------------------

static void before_running_tests() {
    // allocate lists etc
    testing_framework_data.tests_performed = 0;
    testing_framework_data.tests_passed = 0;
    testing_framework_data.tests_failed = 0;
    testing_framework_data.current_test_info = NULL;

    testing_framework_data.info_per_func = create_list();
    testing_framework_data.info_per_test = create_list();
}

static void run_one_test(unit_test_t *test) {
    // prepare, we also need a test_info structure
    struct test_info *ti = kmalloc(sizeof(struct test_info));
    memset(ti, 0, sizeof(struct test_info));
    ti->unit_test = test;
    ti->name = test->func_name;
    ti->passed = true;
    list_add(testing_framework_data.info_per_test, NULL, ti);
    testing_framework_data.current_test_info = ti;
    uint32_t free_heap_before = kernel_heap_free_size();

    // run the test
    test->func();


    if (kernel_heap_free_size() != free_heap_before)
        fail("Memory leak detected!");
    
    // housekeeping
    testing_framework_data.current_test_info = NULL;
    testing_framework_data.tests_performed++;
    if (ti->passed) {
        testing_framework_data.tests_passed++;
        printk(".");
    } else {
        testing_framework_data.tests_failed++;
        printk("F");
    }
}

static void after_running_tests() {

    printk("\n");
    klog_info("%d tests executed, %d passed, %d failed", 
        testing_framework_data.tests_performed,
        testing_framework_data.tests_passed,
        testing_framework_data.tests_failed
    );

    printk("Failed tests:\n");
    list_node_t *node;
    for (node = testing_framework_data.info_per_test->next;
        node != testing_framework_data.info_per_test;
        node = node->next) {
        struct test_info *ti = (struct test_info *)node->payload;
        if (ti->passed)
            continue;

        printk("- %s()\n", ti->name);
        printk("  %s: %s, at %s:%d\n", ti->fail_message, ti->fail_data, ti->fail_file, ti->fail_line);
    }

    // sanity check...
    debug_testing_framework_data();

    for_list(testing_framework_data.info_per_func, p)
        kfree((void *)p->payload);
    for_list(testing_framework_data.info_per_test, p)
        kfree((void *)p->payload);
    
    destroy_list(testing_framework_data.info_per_func, NULL, NULL);
    destroy_list(testing_framework_data.info_per_test, NULL, NULL);
}

bool testing_framework_run_test_suite(unit_test_t tests[], int num_tests) {

    before_running_tests();
    for (int i = 0; i < num_tests; i++) {
        run_one_test(&tests[i]);
    }
    bool all_succeeded = testing_framework_data.tests_failed == 0;
    after_running_tests();

    return all_succeeded;
}

