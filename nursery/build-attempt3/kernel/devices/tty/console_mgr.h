#pragma once
#include "vconsole.h"

// when we go GUI, this will be deprecated, 
// but vconsoles will still live in terminal windows
void init_console_mgr(int num_of_vconsoles);
vconsole_t *console_mgr_get_vconsole(int dev_no);

void vconsole_log_appender(void *context, const char *str);
