#include "reflect.h"
#include "../utils/panic.h"
#include "../klib/string.h"



#define MAX_DESCRIPTIONS  16


struct registry_data {
    const struct_desc_t *descs[MAX_DESCRIPTIONS];
    int count;
};
static struct registry_data reg = { .count = 0 };



void kdebug_register_type(const struct_desc_t *desc) {
    if (reg.count >= MAX_DESCRIPTIONS)
        panic("Cannot register type, all slots already taken. Increase MAX_DESCRIPTIONS");
    
    reg.descs[reg.count] = desc;
    reg.count++;
}

const struct_desc_t *kdebug_find_type_by_name(const char *name) {
    if (name == NULL) return NULL;

    for (int i = 0; i < reg.count; i++) {
        if (strcmp(reg.descs[i]->type_name, name) == 0)
            return reg.descs[i];
    }

    return NULL;
}

const struct_desc_t *kdebug_find_type_by_magic_number(void *instance) {
    if (instance == NULL) return NULL;

    // the first member is supposed to be the magic number
    uint32_t magic = *(uint32_t *)instance;

    for (int i = 0; i < reg.count; i++) {
        if (reg.descs[i]->magic == magic)
            return reg.descs[i];
    }

    return NULL;
}
