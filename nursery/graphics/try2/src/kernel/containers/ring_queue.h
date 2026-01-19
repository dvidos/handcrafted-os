#pragma once

#define DECLARE_RING_QUEUE(name, type, size)                     \
    static type name##_buf[size];                                \
    static int name##_head = 0;                                  \
    static int name##_tail = 0;                                  \
    static int name##_count = 0;                                 \
                                                                 \
    static inline bool name##_has(void) {                        \
        return name##_count > 0;                                 \
    }                                                            \
                                                                 \
    static inline bool name##_is_full(void) {                    \
        return name##_count >= (size);                           \
    }                                                            \
                                                                 \
    static inline bool name##_is_empty(void) {                   \
        return name##_count == 0;                                \
    }                                                            \
                                                                 \
    static inline void name##_enqueue(type v) {                  \
        if (name##_count >= (size)) {                            \
            return; /* or assert */                              \
        }                                                        \
        name##_buf[name##_tail] = v;                             \
        name##_tail = (name##_tail + 1) % (size);                \
        name##_count++;                                          \
    }                                                            \
                                                                 \
    static inline type name##_dequeue(void) {                    \
        type v = (type){0};                                      \
        if (name##_count == 0) {                                 \
            return v;                                            \
        }                                                        \
        v = name##_buf[name##_head];                             \
        name##_head = (name##_head + 1) % (size);                \
        name##_count--;                                          \
        return v;                                                \
    }

