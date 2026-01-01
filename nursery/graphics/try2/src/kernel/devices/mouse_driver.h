#pragma once

#include "../fundamentals.h"

typedef void mouse_xy_retrieval_func(int *x, int *y);
typedef void mouse_xy_update_func(int x, int y);


void initialize_mouse_driver(mouse_xy_retrieval_func *retrieve, mouse_xy_update_func *update);
void mouse_driver_process();
