#pragma once
#include "../include/ctypes.h"


void kdebug_backtrace();
const char* kdebug_get_symbol(uint32_t addr);
