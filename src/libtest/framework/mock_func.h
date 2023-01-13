#ifndef _MOCK_FUNC_H
#define _MOCK_FUNC_H

#include "list.h"
#include "testing.h"


struct mock_func_ops;

typedef struct mock_func_info {
    // a non keyed list of mock values.
    // add with "mock_value()", get with "mocked_value()"
    list_node_t *mock_values_list;

    // a list of expected arguments.
    // key: function & argument name,
    // value: a list of arguments (to enable repeat calls to the injected method)
    list_node_t *expected_args_list;

    struct mock_func_ops *ops;
} mock_func_info_t;


struct mock_func_ops {
    void (*append_mock_value)(mock_func_info_t *info, scalar value);
    void (*append_expected_arg)(mock_func_info_t *info, char *arg_name, scalar expected_value);
    scalar (*dequeue_mock_value)(mock_func_info_t *info, const char *file, int line);
    scalar (*dequeue_expected_arg)(mock_func_info_t *info, char *arg_name, const char *file, int line);
    bool (*all_mock_values_used)(mock_func_info_t *info);
    char *(*get_unchecked_arg_name)(mock_func_info_t *info);
    void (*log_debug_info)(mock_func_info_t *info);
    void (*free)(mock_func_info_t *info);
};


mock_func_info_t *create_mock_func_info();


#endif
