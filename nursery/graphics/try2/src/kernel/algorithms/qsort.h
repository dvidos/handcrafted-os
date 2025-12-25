#pragma once

typedef int (comparator_func)(const void *, const void *);

void qsort(void *base, int item_count, int item_size, comparator_func *compare);
