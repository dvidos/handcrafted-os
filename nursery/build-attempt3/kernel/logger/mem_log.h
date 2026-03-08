#pragma once


void mem_log_appender(void *context, const char *timing, const char *module_name, const char *level, const char *message, bool raw_dump);
const char *mem_log_get_contents();
