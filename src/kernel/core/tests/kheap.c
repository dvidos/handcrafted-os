#include <memory/kheap.h>
#include "framework.h"



void test_kernel_heap() {
    uint32_t block_size = kernel_heap_block_size();
    uint32_t free_mem = kernel_heap_free_size();

    kernel_heap_verify();

    assert(kernel_heap_free_size() == free_mem);
    
    void *p1 = kmalloc(20);
    assert(kernel_heap_free_size() == free_mem - (20 + block_size) * 1);

    void *p2 = kmalloc(20);
    assert(kernel_heap_free_size() == free_mem - (20 + block_size) * 2);

    void *p3 = kmalloc(20);
    assert(kernel_heap_free_size() == free_mem - (20 + block_size) * 3);

    kfree(p1);
    kfree(p3);
    kfree(p2);

    // this ensures merging of freed blocks
    assert(kernel_heap_free_size() == free_mem);

    kernel_heap_verify();
}



