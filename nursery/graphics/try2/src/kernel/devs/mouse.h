#pragma once

#include <stdint.h>

typedef void mouse_xy_retrieval_func(int *x, int *y);
typedef void mouse_xy_update_func(int x, int y);


void initialize_mouse(mouse_xy_retrieval_func *retrieve, mouse_xy_update_func *update);
void mouse_process();
