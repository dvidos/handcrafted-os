#include "../logger/logger.h"
#include "../utils/assert.h"
#include "bitmap.h"
#include "string.h" // For memset

#ifdef ENABLE_UNIT_TESTS

MODULE("BMTST", LOG_LEVEL_INFO);


// Helper to create and initialize a bitmap for tests
static bitmap_t *create_test_bitmap(uint32_t bits, size_t bytes_count) {
    bitmap_t *bm = create_bitmap(bits, bytes_count);
    ASSERT(bm != NULL);
    // Ensure it's initialized to all free
    for (uint32_t i = 0; i < bm->words_count; i++) {
        ASSERT(bm->words[i] == 0);
    }
    return bm;
}

// Test case 1: Creation and Destruction
static void test_creation_destruction() {
    log_info("Running test_creation_destruction...");

    // Test with exact bytes_count
    uint32_t bits1 = 64;
    size_t bytes1 = (bits1 + 7) / 8;
    bitmap_t *bm1 = create_test_bitmap(bits1, bytes1);
    ASSERT(bm1->bits == bits1);
    ASSERT(bm1->words_count == 1);
    ASSERT(bm1->bytes_count == bytes1);
    ASSERT(bm1->bit_hint == 0);
    bm1->ops->destroy(bm1);

    // Test with larger bytes_count
    uint32_t bits2 = 120; // Needs 2 words (128 bits)
    size_t bytes2 = 32; // More than 16 bytes needed for 2 words
    bitmap_t *bm2 = create_test_bitmap(bits2, bytes2);
    ASSERT(bm2->bits == bits2);
    ASSERT(bm2->words_count == 2);
    ASSERT(bm2->bytes_count == bytes2);
    bm2->ops->destroy(bm2);

    // Test with 0 bits (should handle gracefully, maybe return NULL or create empty)
    // Current create_bitmap assumes bits > 0 based on usage for words_count.
    // If bits=0, words_count becomes 0, kmalloc(0) is implementation defined.
    // For now, testing with bits > 0.

    log_info("test_creation_destruction PASSED.");
}

// Test case 2: Basic Bit Operations (mark_used, mark_free, is_used, is_free)
static void test_basic_bit_operations() {
    log_info("Running test_basic_bit_operations...");

    uint32_t bits = 128; // 2 words
    bitmap_t *bm = create_test_bitmap(bits, 0); // 0 for bytes_count will make it minimum
    
    // Test initial state
    for (uint32_t i = 0; i < bits; i++) {
        ASSERT(bm->ops->is_free(bm, i));
        ASSERT(!bm->ops->is_used(bm, i));
    }

    // Mark and check individual bits
    bm->ops->mark_used(bm, 0);
    ASSERT(bm->ops->is_used(bm, 0));
    ASSERT(!bm->ops->is_free(bm, 0));

    bm->ops->mark_used(bm, 63); // Last bit of first word
    ASSERT(bm->ops->is_used(bm, 63));

    bm->ops->mark_used(bm, 64); // First bit of second word
    ASSERT(bm->ops->is_used(bm, 64));

    bm->ops->mark_used(bm, 127); // Last bit of second word
    ASSERT(bm->ops->is_used(bm, 127));

    // Mark free and check
    bm->ops->mark_free(bm, 0);
    ASSERT(bm->ops->is_free(bm, 0));
    ASSERT(!bm->ops->is_used(bm, 0));

    // Ensure other bits are unaffected
    ASSERT(bm->ops->is_used(bm, 63));
    ASSERT(bm->ops->is_used(bm, 64));
    ASSERT(bm->ops->is_used(bm, 127));

    bm->ops->destroy(bm);
    log_info("test_basic_bit_operations PASSED.");
}

// Test case 3: Bulk Operations (mark_all_used, mark_all_free)
static void test_bulk_operations() {
    log_info("Running test_bulk_operations...");

    uint32_t bits = 192; // 3 words
    bitmap_t *bm = create_test_bitmap(bits, 0);

    // Mark all used
    bm->ops->mark_all_used(bm);
    for (uint32_t i = 0; i < bits; i++) {
        ASSERT(bm->ops->is_used(bm, i));
        ASSERT(!bm->ops->is_free(bm, i));
    }
    // Check that words are correctly set to all ones
    for (uint32_t i = 0; i < bm->words_count; i++) {
        ASSERT(bm->words[i] == UINT64_MAX);
    }


    // Mark all free
    bm->ops->mark_all_free(bm);
    for (uint32_t i = 0; i < bits; i++) {
        ASSERT(bm->ops->is_free(bm, i));
        ASSERT(!bm->ops->is_used(bm, i));
    }
    // Check that words are correctly set to all zeros
    for (uint32_t i = 0; i < bm->words_count; i++) {
        ASSERT(bm->words[i] == 0);
    }

    bm->ops->destroy(bm);
    log_info("test_bulk_operations PASSED.");
}

// Test case 4: find_next_free
static void test_find_next_free() {
    log_info("Running test_find_next_free...");

    uint32_t bits = 128; // 2 words
    bitmap_t *bm = create_test_bitmap(bits, 0);
    uint32_t found_bit;

    // Find first free bit (should be 0)
    ASSERT(bm->ops->find_next_free(bm, &found_bit));
    ASSERT(found_bit == 0);
    bm->ops->mark_used(bm, 0); // Mark it used for next search

    // Find next free bit (should be 1)
    ASSERT(bm->ops->find_next_free(bm, &found_bit));
    ASSERT(found_bit == 1);
    bm->ops->mark_used(bm, 1);

    // Mark a bit in the middle of a word
    bm->ops->mark_used(bm, 30);
    bm->ops->mark_used(bm, 31);
    ASSERT(bm->ops->find_next_free(bm, &found_bit));
    ASSERT(found_bit == 2); // Still finds 2

    // Fill up the first word
    for (uint32_t i = 0; i < 64; i++) {
        if (i == 0 || i == 1) continue; // Already marked
        bm->ops->mark_used(bm, i);
    }
    ASSERT(!bm->ops->is_free(bm, 63)); // Ensure last bit is used
    
    // Should find first bit in second word (64)
    ASSERT(bm->ops->find_next_free(bm, &found_bit));
    ASSERT(found_bit == 64);
    bm->ops->mark_used(bm, 64);

    // Mark all used
    bm->ops->mark_all_used(bm);
    ASSERT(!bm->ops->find_next_free(bm, &found_bit)); // Should return false, no free bits

    // Test wraparound (set some bits, then search from hint that's past free bits)
    bm->ops->mark_all_free(bm);
    bm->ops->mark_used(bm, 0);
    bm->ops->mark_used(bm, 1);
    for (uint32_t i = 10; i < bits; i++) { // Mark most bits from 10 onwards
        bm->ops->mark_used(bm, i);
    }

    bm->bit_hint = 100; // Set hint past the free bits (2 to 9)
    ASSERT(bm->ops->find_next_free(bm, &found_bit));
    ASSERT(found_bit == 2); // Should wraparound and find 2
    ASSERT(bm->bit_hint == 3); // hint updated to next bit

    bm->ops->destroy(bm);
    log_info("test_find_next_free PASSED.");
}

// Test case 5: Edge cases
static void test_edge_cases() {
    log_info("Running test_edge_cases...");

    uint32_t bits_partial_word = 70; // Fills 1 word + 6 bits in the next
    bitmap_t *bm_partial = create_test_bitmap(bits_partial_word, 0);
    uint32_t found_bit;

    // Test marking/checking bits in the partial word
    bm_partial->ops->mark_used(bm_partial, 69);
    ASSERT(bm_partial->ops->is_used(bm_partial, 69));
    ASSERT(bm_partial->ops->is_free(bm_partial, 70)); // Out of bounds, should be safe
    // The implementation of is_free/is_used will access words[70/64] which is words[1]
    // and (70 % 64) which is 6. This is within the allocated memory words[1]
    // but beyond bm->bits. The check for index < bm->bits is in find_next_free.
    // In is_used/is_free, it implicitly assumes bit < bm->bits.
    // ASSERT(bm_partial->ops->is_used(bm_partial, bits_partial_word)); // This would be problematic if not handled

    // Verify words_count is correct
    ASSERT(bm_partial->words_count == 2); // (70+63)/64 = 2

    // Check last valid bit
    ASSERT(bm_partial->ops->is_free(bm_partial, 68));
    bm_partial->ops->mark_used(bm_partial, 68);
    ASSERT(bm_partial->ops->is_used(bm_partial, 68));

    // Test find_next_free with partial word
    bm_partial->ops->mark_all_used(bm_partial);
    ASSERT(!bm_partial->ops->find_next_free(bm_partial, &found_bit)); // All valid bits used
    
    bm_partial->ops->mark_all_free(bm_partial);
    for (uint32_t i = 0; i < bits_partial_word; i++) {
        bm_partial->ops->mark_used(bm_partial, i);
    }
    ASSERT(!bm_partial->ops->find_next_free(bm_partial, &found_bit)); // All valid bits used

    bm_partial->ops->destroy(bm_partial);
    
    log_info("test_edge_cases PASSED.");
}

void bitmap_unit_tests() {
    log_info("Running bitmap unit tests...");

    test_creation_destruction();
    test_basic_bit_operations();
    test_bulk_operations();
    test_find_next_free();
    test_edge_cases();

    log_info("All bitmap unit tests completed.");
}

#endif // ENABLE_UNIT_TESTS
