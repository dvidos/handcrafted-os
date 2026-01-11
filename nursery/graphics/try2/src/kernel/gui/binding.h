#pragma once

// these bindings per type, to avoid hooking to each widget's different logic.


typedef void (string_binding_on_changed_func)(const char *new_value, void *context);
typedef void (int_binding_on_changed_func)(int new_value, void *context);
typedef void (bool_binding_on_changed_func)(bool new_value, void *context);
typedef void (enum_binding_on_changed_func)(void *new_value, void *context);
typedef void (action_binding_on_triggered_func)(void *context);

typedef struct string_binding {
    const char *(*get)(struct string_binding *b);
    void (*set)(struct string_binding *b, const char *value);
} string_binding_t;

typedef struct string_binding_private {
    string_binding_t public_;
    const char *value;
    const char *context;
    string_binding_on_changed_func *on_changed;
} string_binding_private_t;



typedef struct int_binding {
    int (*get)(struct int_binding *b);
    void (*set)(struct int_binding *b, int value);
} int_binding_t;

typedef struct int_binding_private {
    // ...
} int_binding_private_t;



typedef struct bool_binding {
    bool (*get)(struct bool_binding *b);
    void (*set)(struct bool_binding *b, bool value);
} bool_binding_t;

typedef struct bool_binding_private {
    // ...
} bool_binding_private_t;




typedef struct enum_binding {
    int (*get)(struct enum_binding *b);
    void (*set)(struct enum_binding *b, int value);
} enum_binding_t;

typedef struct enum_binding_private {
} enum_binding_private_t;




typedef struct action_binding {
    void (*trigger)(struct bool_binding *b);
} action_binding_t;



string_binding_t *new_string_binding(string_binding_on_changed_func *on_changed, void *context);
int_binding_t *new_int_binding(int_binding_on_changed_func *on_changed, void *context);
bool_binding_t *new_bool_binding(bool_binding_on_changed_func *on_changed, void *context);
enum_binding_t *new_enum_binding(enum_binding_on_changed_func *on_changed, void *context, const char **captions_arr, int captions_count);
action_binding_t *new_action_binding(action_binding_on_triggered_func *on_triggered, void *context);

