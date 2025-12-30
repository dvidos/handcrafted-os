#pragma once

typedef enum logger_level logger_level; 
enum logger_level {
    LOG_LEVEL_TRACE = 1,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_PANIC,
};

typedef struct logger_methods {
    void (*trace)(const char *fmt, ...);
    void (*debug)(const char *fmt, ...);
    void (*info)(const char *fmt, ...);
    void (*warn)(const char *fmt, ...);
    void (*error)(const char *fmt, ...);
    void (*panic)(const char *fmt, ...);
} logger_methods;

void initialize_logger();
logger_level logger_get_level();
void logger_set_level(logger_level new_level);

extern logger_methods log;


#define LOG_TRACE()    log.trace("%s:%d:%s()", __FILE__, __LINE__, __func__)