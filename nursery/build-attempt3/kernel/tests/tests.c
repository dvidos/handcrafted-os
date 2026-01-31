#include <ctypes.h>
#include <errors.h>
#include <string.h>
#include <drivers/screen.h>
#include "framework.h"
#include <klog.h>
#include <memory/kheap.h>

MODULE("UNITTEST");

void test_kernel_heap();
void test_paths();
void test_printf();
void test_strbuff();
void test_strings();
void test_vfs();


// final code to be run
bool run_frameworked_unit_tests() {
    unit_test_t tests[] = {
        unit_test(test_kernel_heap),
        unit_test(test_paths),
        unit_test(test_printf),
        unit_test(test_strbuff),
        unit_test(test_strings),
        // unit_test(test_vfs),
    };
    return run_tests(tests);
}
