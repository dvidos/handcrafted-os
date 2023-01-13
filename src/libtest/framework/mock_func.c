#include <stddef.h>
#include "../include/testing.h"
#include "setup.h"
#include "list.h"
#include "mock_func.h"

static void mock_func_append_mock_value(mock_func_info_t *info, int value);
static void mock_func_append_expected_arg(mock_func_info_t *info, char *arg_name, int expected_value);
static int mock_func_dequeue_mock_value(mock_func_info_t *info, const char *file, int line);
static int mock_func_dequeue_expected_arg(mock_func_info_t *info, char *arg_name, const char *file, int line);
static bool mock_func_all_mock_values_used(mock_func_info_t *info);
static char *mock_func_get_unchecked_arg_name(mock_func_info_t *info);
static void mock_func_log_debug_info(mock_func_info_t *info);
static void mock_func_info_free(mock_func_info_t *info);

struct mock_func_ops mock_func_ops = {
    .append_mock_value = mock_func_append_mock_value,
    .append_expected_arg = mock_func_append_expected_arg,
    .dequeue_mock_value = mock_func_dequeue_mock_value,
    .dequeue_expected_arg = mock_func_dequeue_expected_arg,
    .all_mock_values_used = mock_func_all_mock_values_used,
    .get_unchecked_arg_name = mock_func_get_unchecked_arg_name,
    .log_debug_info = mock_func_log_debug_info,
    .free = mock_func_info_free
};

mock_func_info_t *create_mock_func_info() {
    mock_func_info_t *info = testing_framework_setup.malloc(sizeof(mock_func_info_t));
    info->mock_values_list = create_list();
    info->expected_args_list = create_list();
    info->ops = &mock_func_ops;
    return info;
}

static void mock_func_append_mock_value(mock_func_info_t *info, int value) {
    info->mock_values_list->ops->add(info->mock_values_list, NULL, (void *)value);
}

static void mock_func_append_expected_arg(mock_func_info_t *info, char *arg_name, int expected_value) {
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
static int mock_func_dequeue_mock_value(mock_func_info_t *info, const char *file, int line) {
    list_node_t *first = info->mock_values_list->ops->first(info->mock_values_list);
    if (first == NULL)
        _testing_framework_test_failed("Mock value requested, but not setup", NULL, file, line);
    int value = (int)first->payload;
    info->mock_values_list->ops->remove(info->mock_values_list, first, NULL, NULL);
    return value;
}

static int mock_func_dequeue_expected_arg(mock_func_info_t *info, char *arg_name, const char *file, int line) {
    // each node has a list of expected values for this argument.
    list_node_t *arg_node = info->expected_args_list->ops->find_key(info->expected_args_list, arg_name);
    if (arg_node == NULL)
        _testing_framework_test_failed("No arg list found for this argument", arg_name, file, line);
    
    list_node_t *values_list = (list_node_t *)arg_node->payload;
    list_node_t *first = values_list->ops->first(values_list);
    if (first == NULL)
        _testing_framework_test_failed("Arg list for this argument is empty", arg_name, file, line);

    int value = (int)first->payload;
    values_list->ops->remove(values_list, first, NULL, NULL);
    return value;
}

static bool mock_func_all_mock_values_used(mock_func_info_t *info) {
    return info->mock_values_list->ops->empty(info->mock_values_list);
}

static char *mock_func_get_unchecked_arg_name(mock_func_info_t *info) {
    for_list(arg_node,  info->expected_args_list) {
        list_node_t *values_list = (list_node_t *)arg_node->payload;
        if (!values_list->ops->empty(values_list))
            return arg_node->key;
    }
    return NULL;
}

static void mock_func_log_debug_info(mock_func_info_t *info) {
    if (!info->mock_values_list->ops->empty(info->mock_values_list)) {
        testing_framework_setup.printf("      Mocked values:");
        for_list(node, info->mock_values_list) {
            log_debug_any_value("      - ", node->payload);
        }
    }

    if (!info->expected_args_list->ops->empty(info->expected_args_list)) {
        testing_framework_setup.printf("      Arguments validations:");
        for_list(arg_node,  info->expected_args_list) {
            testing_framework_setup.printf("      - \"%s\"", arg_node->key);
            list_node_t *values_list = (list_node_t *)arg_node->payload;
            for_list(val_node, values_list) {
                log_debug_any_value("        - ", val_node->payload);
            }
        }
    }
}

static void mock_func_info_free(mock_func_info_t *info) {
    info->mock_values_list->ops->free(info->mock_values_list, NULL, NULL);

    for_list (args_list, info->expected_args_list) {
        list_node_t *values_list = (list_node_t *)args_list->payload;
        values_list->ops->debug(values_list);
        values_list->ops->free(values_list, NULL, NULL);
    }

    info->expected_args_list->ops->free(info->expected_args_list, NULL, NULL);
    testing_framework_setup.free(info);
}

