#ifndef REFLECT_H
#define REFLECT_H

#include "../include/ctypes.h"

/**
 * This module to allow one to describe their structures,
 * in a way that our debugger could traverse them.
 *     Example:
 *     
 *     typedef struct {
 *         uint32_t magic;
 *         char name[16];
 *         void *data;
 *     } sample_t;
 *     
 *     static const field_desc_t sample_fields[] = {
 *         DESC_UINT32(sample_t, magic),
 *         DESC_STR(sample_t, name, 16),
 *         DESC_PTR(sample_t, data)
 *     };
 *     DESCRIBE_STRUCT(sample_t, 0x12345678, sample_fields);
 *     
 *     debug_register_type(&sample_t_desc);
 */

typedef enum {
    TYPE_UINT32,
    TYPE_UINT64,
    TYPE_PTR,         // Generic (void *)
    TYPE_CHAR_ARR,    // Null-terminated char array
    TYPE_STRUCT_PTR,  // Pointer to another described struct
    TYPE_PTR_ARRAY,   // Fixed-size array of pointers to structs
    TYPE_STRUCT       // Nested (inlined) struct
} field_type_t;

typedef struct struct_desc_t struct_desc_t;

typedef struct {
    const char *name;
    field_type_t type;
    size_t size;
    size_t offset;
    
    // Metadata for complex types
    const struct_desc_t *target_desc;
    size_t array_count; 
} field_desc_t;

struct struct_desc_t {
    const char *type_name;
    uint32_t magic;
    size_t size;
    const field_desc_t *fields;
    size_t field_count;
};


#define DESC_UINT32(s_type, field) \
    { #field, TYPE_UINT32, sizeof(uint32_t), offsetof(s_type, field), NULL, 0 }

#define DESC_UINT64(s_type, field) \
    { #field, TYPE_UINT64, sizeof(uint64_t), offsetof(s_type, field), NULL, 0 }

#define DESC_PTR(s_type, field) \
    { #field, TYPE_PTR, sizeof(void*), offsetof(s_type, field), NULL, 0 }

#define DESC_CHAR_ARR(s_type, field, max_len) \
    { #field, TYPE_CHAR_ARR, max_len, offsetof(s_type, field), NULL, 0 }

#define DESC_STRUCT_PTR(s_type, field, target_s_desc) \
    { #field, TYPE_STRUCT_PTR, sizeof(void*), offsetof(s_type, field), &target_s_desc, 0 }

#define DESC_PTR_ARRAY(s_type, field, target_s_desc, count) \
    { #field, TYPE_PTR_ARRAY, sizeof(void*), offsetof(s_type, field), &target_s_desc, count }

#define DESC_STRUCT(s_type, field, target_s_desc) \
    { #field, TYPE_STRUCT, sizeof(((s_type*)0)->field), offsetof(s_type, field), &target_s_desc, 0 }

/**
 * DESCRIBE_STRUCT
 * Creates a struct_desc_t object for a given structure.
 * s_type: The typedef name of the struct
 * magic_val: The 32-bit magic number expected at offset 0
 * field_array: The static array of field_desc_t
 */
#define DESCRIBE_STRUCT(s_type, magic_val, field_array) \
    static const struct_desc_t s_type##_desc = { \
        .type_name = #s_type, \
        .magic = magic_val, \
        .size = sizeof(s_type), \
        .fields = field_array, \
        .field_count = sizeof(field_array) / sizeof(field_desc_t) \
    }


// Call this in your module init to make the type visible to the debugger
void kdebug_register_type(const struct_desc_t *desc);
const struct_desc_t *kdebug_find_type_by_name(const char *name);
const struct_desc_t *kdebug_find_type_by_magic_number(void *instance);




#endif // REFLECT_H
