#include "../logger/logger.h"
#include "../utils/assert.h"
#include "backed_cache.h"
#include "string.h" // For memcpy, memset

#ifdef ENABLE_UNIT_TESTS

MODULE("BCUT", LOG_LEVEL_INFO);


// Mock backend data
#define MOCK_BACKEND_DATA_SIZE 10
#define TEST_OBJ_SIZE 4 // Small object size for testing
#define TEST_CACHE_CAPACITY 5 // Capacity for tests

typedef struct {
    uint8_t data[MOCK_BACKEND_DATA_SIZE][TEST_OBJ_SIZE];
    bool loaded[MOCK_BACKEND_DATA_SIZE];
    bool written[MOCK_BACKEND_DATA_SIZE];
    int load_count;
    int write_count;
    // New fields for error injection
    uint64_t load_fail_key;
    uint64_t write_fail_key;
} mock_backend_context_t;

static error_t mock_backend_load(uint64_t key, void *obj_data, void *context) {
    mock_backend_context_t *mock_ctx = (mock_backend_context_t *)context;
    if (key == mock_ctx->load_fail_key) {
        log_error("Mock backend simulating load failure for key %llu", key);
        return ERR_IO_ERROR; // Simulate an I/O error
    }
    if (key >= MOCK_BACKEND_DATA_SIZE) return ERR_INVALID_ARGS;
    
    // Simulate loading data
    memcpy(obj_data, mock_ctx->data[key], TEST_OBJ_SIZE);
    mock_ctx->loaded[key] = true;
    mock_ctx->load_count++;
    log_info("Mock backend loaded key %llu", key);
    return OK;
}

static error_t mock_backend_write(uint64_t key, void *obj_data, void *context) {
    mock_backend_context_t *mock_ctx = (mock_backend_context_t *)context;
    if (key == mock_ctx->write_fail_key) {
        log_error("Mock backend simulating write failure for key %llu", key);
        return ERR_IO_ERROR; // Simulate an I/O error
    }
    if (key >= MOCK_BACKEND_DATA_SIZE) return ERR_INVALID_ARGS;

    // Simulate writing data
    memcpy(mock_ctx->data[key], obj_data, TEST_OBJ_SIZE);
    mock_ctx->written[key] = true;
    mock_ctx->write_count++;
    log_info("Mock backend wrote key %llu", key);
    return OK;
}

// Helper to reset mock context
static void reset_mock_context(mock_backend_context_t *mock_ctx) {
    memset(mock_ctx, 0, sizeof(mock_backend_context_t));
    // Initialize some dummy data in the backend
    for (int i = 0; i < MOCK_BACKEND_DATA_SIZE; ++i) {
        for (int j = 0; j < TEST_OBJ_SIZE; ++j) {
            mock_ctx->data[i][j] = (uint8_t)(i + j);
        }
    }
    mock_ctx->load_fail_key = (uint64_t)-1; // No fail by default
    mock_ctx->write_fail_key = (uint64_t)-1; // No fail by default
}

// Helper to create a test cache
static backed_cache_t *create_test_cache(size_t obj_capacity, mock_backend_context_t *mock_ctx) {
    backed_cache_backend backend = {
        .load = mock_backend_load,
        .write = mock_backend_write,
        .context = mock_ctx
    };
    return create_backed_cache(TEST_OBJ_SIZE, obj_capacity, backend);
}

// Test case 1: Basic creation and destruction
static void test_creation_destruction() {
    log_info("Running test_creation_destruction...");
    mock_backend_context_t mock_ctx;
    reset_mock_context(&mock_ctx);

    backed_cache_t *cache = create_test_cache(TEST_CACHE_CAPACITY, &mock_ctx);
    ASSERT(cache != NULL);
    ASSERT(cache->obj_size == TEST_OBJ_SIZE);
    ASSERT(cache->obj_capacity == TEST_CACHE_CAPACITY);
    ASSERT(cache->used_count == 0);
    ASSERT(cache->dirty_count == 0);
    ASSERT(cache->references_sum == 0);
    ASSERT(cache->backend.load == mock_backend_load);
    ASSERT(cache->backend.write == mock_backend_write);
    ASSERT(cache->backend.context == &mock_ctx);

    error_t err = cache->ops->destroy(cache);
    ASSERT(err == OK);
    // Note: Can't easily verify freed memory here without custom allocators
    log_info("test_creation_destruction PASSED.");
}

// Test case 2: Basic Get/Load Functionality (Cache Miss/Hit)
static void test_get_load_functionality() {
    log_info("Running test_get_load_functionality...");
    mock_backend_context_t mock_ctx;
    reset_mock_context(&mock_ctx);

    backed_cache_t *cache = create_test_cache(TEST_CACHE_CAPACITY, &mock_ctx);
    ASSERT(cache != NULL);

    void *obj_ptr;
    error_t err;
    uint64_t key1 = 0;
    uint64_t key2 = 1;

    // --- Test 1: Cache Miss for key1 ---
    err = cache->ops->get(cache, key1, &obj_ptr);
    ASSERT(err == OK);
    ASSERT(mock_ctx.load_count == 1);
    ASSERT(mock_ctx.loaded[key1] == true);
    // Verify content
    for (int i = 0; i < TEST_OBJ_SIZE; ++i) {
        ASSERT(((uint8_t*)obj_ptr)[i] == mock_ctx.data[key1][i]);
    }
    ASSERT(cache->used_count == 1);
    ASSERT(cache->lru_newest->key == key1); // key1 should be newest

    // --- Test 2: Cache Hit for key1 ---
    err = cache->ops->get(cache, key1, &obj_ptr);
    ASSERT(err == OK);
    ASSERT(mock_ctx.load_count == 1); // Should not load again
    // Verify content (should be the same)
    for (int i = 0; i < TEST_OBJ_SIZE; ++i) {
        ASSERT(((uint8_t*)obj_ptr)[i] == mock_ctx.data[key1][i]);
    }
    ASSERT(cache->used_count == 1); // Still 1 used item
    ASSERT(cache->lru_newest->key == key1); // key1 should still be newest

    // --- Test 3: Cache Miss for key2 ---
    err = cache->ops->get(cache, key2, &obj_ptr);
    ASSERT(err == OK);
    ASSERT(mock_ctx.load_count == 2); // Loaded key2
    ASSERT(mock_ctx.loaded[key2] == true);
    // Verify content
    for (int i = 0; i < TEST_OBJ_SIZE; ++i) {
        ASSERT(((uint8_t*)obj_ptr)[i] == mock_ctx.data[key2][i]);
    }
    ASSERT(cache->used_count == 2);
    ASSERT(cache->lru_newest->key == key2); // key2 should be newest
    ASSERT(cache->lru_newest->lru_older->key == key1); // key1 should be older

    // Verify LRU order (key1 should be oldest now)
    ASSERT(cache->lru_oldest->key == key1);
    ASSERT(cache->lru_oldest->lru_newer->key == key2);
    
    cache->ops->destroy(cache);
    log_info("test_get_load_functionality PASSED.");
}

// Test case 3: Dirty Tracking
static void test_dirty_tracking() {
    log_info("Running test_dirty_tracking...");
    mock_backend_context_t mock_ctx;
    reset_mock_context(&mock_ctx);

    backed_cache_t *cache = create_test_cache(TEST_CACHE_CAPACITY, &mock_ctx);
    ASSERT(cache != NULL);

    void *obj_ptr;
    error_t err;
    uint64_t key = 0;
    uint8_t test_data[TEST_OBJ_SIZE] = {0xAA, 0xBB, 0xCC, 0xDD};

    // --- Test 1: Mark dirty after get ---
    err = cache->ops->get(cache, key, &obj_ptr);
    ASSERT(err == OK);
    ASSERT(cache->ops->is_dirty(cache, key) == false);
    ASSERT(cache->dirty_count == 0);

    err = cache->ops->mark_dirty(cache, key);
    ASSERT(err == OK);
    ASSERT(cache->ops->is_dirty(cache, key) == true);
    ASSERT(cache->dirty_count == 1);

    // Marking dirty again should not increment count
    err = cache->ops->mark_dirty(cache, key);
    ASSERT(err == OK);
    ASSERT(cache->ops->is_dirty(cache, key) == true);
    ASSERT(cache->dirty_count == 1);

    // --- Test 2: Write to item ---
    uint64_t key_write = 1;
    err = cache->ops->write(cache, key_write, test_data);
    ASSERT(err == OK);
    ASSERT(cache->ops->is_dirty(cache, key_write) == true);
    ASSERT(cache->dirty_count == 2); // key 0 and key 1 are dirty

    // Writing again should not increment count
    err = cache->ops->write(cache, key_write, test_data);
    ASSERT(err == OK);
    ASSERT(cache->ops->is_dirty(cache, key_write) == true);
    ASSERT(cache->dirty_count == 2);

    // --- Test 3: Fill item ---
    uint64_t key_fill = 2;
    err = cache->ops->fill(cache, key_fill, 0xBE);
    ASSERT(err == OK);
    ASSERT(cache->ops->is_dirty(cache, key_fill) == true);
    ASSERT(cache->dirty_count == 3);

    // --- Test 4: Write part to item ---
    uint64_t key_write_part = 3;
    uint8_t part_data = 0x12;
    err = cache->ops->write_part(cache, key_write_part, 0, &part_data, 1);
    ASSERT(err == OK);
    ASSERT(cache->ops->is_dirty(cache, key_write_part) == true);
    ASSERT(cache->dirty_count == 4);

    // --- Test 5: Fill part of item ---
    uint64_t key_fill_part = 4;
    err = cache->ops->fill_part(cache, key_fill_part, 0, 0xEF, 1);
    ASSERT(err == OK);
    ASSERT(cache->ops->is_dirty(cache, key_fill_part) == true);
    ASSERT(cache->dirty_count == 5);

    cache->ops->destroy(cache);
    log_info("test_dirty_tracking PASSED.");
}

// Test case 4: Flush Functionality
static void test_flush_functionality() {
    log_info("Running test_flush_functionality...");
    mock_backend_context_t mock_ctx;
    reset_mock_context(&mock_ctx);

    backed_cache_t *cache = create_test_cache(TEST_CACHE_CAPACITY, &mock_ctx);
    ASSERT(cache != NULL);

    void *obj_ptr;
    error_t err;
    uint64_t key1 = 0, key2 = 1, key3 = 2;
    uint8_t test_data_key1[TEST_OBJ_SIZE] = {0x11, 0x22, 0x33, 0x44};
    uint8_t test_data_key2[TEST_OBJ_SIZE] = {0xAA, 0xBB, 0xCC, 0xDD};

    // --- Test 1: Flush a dirty item ---
    err = cache->ops->get(cache, key1, &obj_ptr);
    ASSERT(err == OK);
    memcpy(obj_ptr, test_data_key1, TEST_OBJ_SIZE); // Modify data
    err = cache->ops->mark_dirty(cache, key1);
    ASSERT(err == OK);
    ASSERT(cache->ops->is_dirty(cache, key1) == true);
    ASSERT(cache->dirty_count == 1);
    ASSERT(mock_ctx.write_count == 0);

    err = cache->ops->flush(cache, key1);
    ASSERT(err == OK);
    ASSERT(cache->ops->is_dirty(cache, key1) == false);
    ASSERT(cache->dirty_count == 0);
    ASSERT(mock_ctx.write_count == 1);
    // Verify data in mock backend
    for (int i = 0; i < TEST_OBJ_SIZE; ++i) {
        ASSERT(mock_ctx.data[key1][i] == test_data_key1[i]);
    }

    // --- Test 2: Flush a clean item ---
    err = cache->ops->flush(cache, key1); // Should do nothing
    ASSERT(err == OK);
    ASSERT(cache->ops->is_dirty(cache, key1) == false);
    ASSERT(cache->dirty_count == 0);
    ASSERT(mock_ctx.write_count == 1); // Should still be 1

    // --- Test 3: Flush all ---
    mock_ctx.write_count = 0; // Reset write count for this sub-test

    // Make key1 dirty again
    err = cache->ops->get(cache, key1, &obj_ptr);
    ASSERT(err == OK);
    ((uint8_t*)obj_ptr)[0] = 0xFF;
    err = cache->ops->mark_dirty(cache, key1);
    ASSERT(err == OK);

    // Get key2 and make it dirty
    err = cache->ops->get(cache, key2, &obj_ptr);
    ASSERT(err == OK);
    memcpy(obj_ptr, test_data_key2, TEST_OBJ_SIZE);
    err = cache->ops->mark_dirty(cache, key2);
    ASSERT(err == OK);

    // Get key3 (clean)
    err = cache->ops->get(cache, key3, &obj_ptr);
    ASSERT(err == OK);

    ASSERT(cache->dirty_count == 2);
    ASSERT(cache->ops->is_dirty(cache, key1) == true);
    ASSERT(cache->ops->is_dirty(cache, key2) == true);
    ASSERT(cache->ops->is_dirty(cache, key3) == false);

    err = cache->ops->flush_all(cache);
    ASSERT(err == OK);
    ASSERT(cache->dirty_count == 0);
    ASSERT(cache->ops->is_dirty(cache, key1) == false);
    ASSERT(cache->ops->is_dirty(cache, key2) == false);
    ASSERT(mock_ctx.write_count == 2); // Should have written key1 and key2

    // --- Test 4: Flush when backend.write is NULL ---
    backed_cache_backend no_write_backend = {
        .load = mock_backend_load,
        .write = NULL,
        .context = &mock_ctx
    };
    backed_cache_t *cache_no_write = create_backed_cache(TEST_OBJ_SIZE, TEST_CACHE_CAPACITY, no_write_backend);
    ASSERT(cache_no_write != NULL);

    err = cache_no_write->ops->get(cache_no_write, key1, &obj_ptr);
    ASSERT(err == OK);
    err = cache_no_write->ops->mark_dirty(cache_no_write, key1);
    ASSERT(err == OK);

    err = cache_no_write->ops->flush(cache_no_write, key1);
    ASSERT(err == ERR_NOT_SUPPORTED);
    err = cache_no_write->ops->flush_all(cache_no_write);
    ASSERT(err == ERR_NOT_SUPPORTED);

    cache_no_write->ops->destroy(cache_no_write);
    cache->ops->destroy(cache);
    log_info("test_flush_functionality PASSED.");
}

// Test case 5: Lock/Unlock/Reference Counting
static void test_lock_unlock_functionality() {
    log_info("Running test_lock_unlock_functionality...");
    mock_backend_context_t mock_ctx;
    reset_mock_context(&mock_ctx);

    backed_cache_t *cache = create_test_cache(TEST_CACHE_CAPACITY, &mock_ctx);
    ASSERT(cache != NULL);

    void *obj_ptr;
    error_t err;
    uint64_t key = 0;

    // --- Test 1: Get an item, then lock it ---
    err = cache->ops->get(cache, key, &obj_ptr);
    ASSERT(err == OK);
    ASSERT(cache->ops->is_locked(cache, key) == false);
    ASSERT(cache->references_sum == 0);

    err = cache->ops->lock(cache, key);
    ASSERT(err == OK);
    ASSERT(cache->references_sum == 1);
    ASSERT(cache->ops->is_locked(cache, key) == true);

    // --- Test 2: Lock the same item again ---
    err = cache->ops->lock(cache, key);
    ASSERT(err == OK);
    ASSERT(cache->references_sum == 2);
    ASSERT(cache->ops->is_locked(cache, key) == true);

    // --- Test 3: Unlock the item once ---
    err = cache->ops->unlock(cache, key);
    ASSERT(err == OK);
    ASSERT(cache->references_sum == 1);
    ASSERT(cache->ops->is_locked(cache, key) == true);

    // --- Test 4: Unlock the item completely ---
    err = cache->ops->unlock(cache, key);
    ASSERT(err == OK);
    ASSERT(cache->references_sum == 0);
    ASSERT(cache->ops->is_locked(cache, key) == false);

    // --- Test 5: Attempt to unlock a non-referenced item (should be safe) ---
    // The current implementation of unlock decrements ref_count only if > 0.
    // If ref_count is already 0, it just promotes the node.
    // So, calling unlock on an unreferenced item is harmless.
    err = cache->ops->unlock(cache, key);
    ASSERT(err == OK);
    ASSERT(cache->references_sum == 0); // Should be cache->references_sum == 0
    ASSERT(cache->ops->is_locked(cache, key) == false);

    // --- Test 6: Lock an item not in cache, then unlock ---
    uint64_t new_key = 5;
    ASSERT(cache->ops->is_locked(cache, new_key) == false);
    err = cache->ops->lock(cache, new_key); // This should load it into cache and lock
    ASSERT(err == OK);
    // Verify it's in cache and locked
    ASSERT(cache->references_sum == 1); // Only new_key is locked
    ASSERT(cache->ops->is_locked(cache, new_key) == true);

    err = cache->ops->unlock(cache, new_key);
    ASSERT(err == OK);
    ASSERT(cache->references_sum == 0);
    ASSERT(cache->ops->is_locked(cache, new_key) == false);

    cache->ops->destroy(cache);
    log_info("test_lock_unlock_functionality PASSED.");
}

// Test case 6: LRU Eviction
static void test_lru_eviction() {
    log_info("Running test_lru_eviction...");
    mock_backend_context_t mock_ctx;
    reset_mock_context(&mock_ctx);

    // Use a small cache capacity for easier eviction testing
    size_t small_capacity = 2;
    backed_cache_t *cache = create_test_cache(small_capacity, &mock_ctx);
    ASSERT(cache != NULL);

    void *obj_ptr;
    error_t err;
    uint64_t key0 = 0, key1 = 1, key2 = 2; // Keys for testing

    // --- Test 1: Fill the cache ---
    err = cache->ops->get(cache, key0, &obj_ptr); // LRU: K0
    ASSERT(err == OK);
    ASSERT(cache->used_count == 1);
    ASSERT(mock_ctx.load_count == 1);

    err = cache->ops->get(cache, key1, &obj_ptr); // LRU: K0, K1
    ASSERT(err == OK);
    ASSERT(cache->used_count == 2);
    ASSERT(mock_ctx.load_count == 2);
    ASSERT(cache->lru_oldest->key == key0);
    ASSERT(cache->lru_newest->key == key1);

    // --- Test 2: Evict clean, unreferenced item (key0) ---
    err = cache->ops->get(cache, key2, &obj_ptr); // LRU: K1, K2 (K0 evicted)
    ASSERT(err == OK);
    ASSERT(cache->used_count == 2); // Still 2, K0 was evicted
    ASSERT(mock_ctx.load_count == 3); // K2 loaded
    ASSERT(mock_ctx.written[key0] == false); // K0 was clean, not written back
    ASSERT(cache->lru_oldest->key == key1);
    ASSERT(cache->lru_newest->key == key2);

    // Try to get key0, it should be reloaded
    err = cache->ops->get(cache, key0, &obj_ptr); // LRU: K2, K0 (K1 evicted)
    ASSERT(err == OK);
    ASSERT(mock_ctx.load_count == 4); // K0 reloaded
    ASSERT(mock_ctx.written[key1] == false); // K1 was clean, not written back
    ASSERT(cache->lru_oldest->key == key2);
    ASSERT(cache->lru_newest->key == key0);


    // --- Test 3: Evict dirty, unreferenced item ---
    // Reset cache and mock context for this sub-test
    cache->ops->destroy(cache);
    reset_mock_context(&mock_ctx);
    cache = create_test_cache(small_capacity, &mock_ctx);
    ASSERT(cache != NULL);

    err = cache->ops->get(cache, key0, &obj_ptr);
    ASSERT(err == OK);
    err = cache->ops->mark_dirty(cache, key0); // Make K0 dirty
    ASSERT(err == OK);
    ASSERT(cache->dirty_count == 1);
    ASSERT(mock_ctx.written[key0] == false);

    err = cache->ops->get(cache, key1, &obj_ptr);
    ASSERT(err == OK);

    err = cache->ops->get(cache, key2, &obj_ptr); // K0 should be evicted, written back
    ASSERT(err == OK);
    ASSERT(mock_ctx.written[key0] == true); // K0 was dirty, should be written
    ASSERT(cache->dirty_count == 0); // Dirty count should reset after write
    ASSERT(cache->lru_oldest->key == key1); // K1 is now oldest
    ASSERT(cache->lru_newest->key == key2); // K2 is newest

    // --- Test 4: Cannot evict locked item ---
    cache->ops->destroy(cache);
    reset_mock_context(&mock_ctx);
    cache = create_test_cache(small_capacity, &mock_ctx);
    ASSERT(cache != NULL);

    err = cache->ops->get(cache, key0, &obj_ptr);
    ASSERT(err == OK);
    err = cache->ops->lock(cache, key0); // Lock K0
    ASSERT(err == OK);
    ASSERT(cache->references_sum == 1);

    err = cache->ops->get(cache, key1, &obj_ptr);
    ASSERT(err == OK); // Cache is full (K0 locked, K1 used)

    // Attempt to get another item. K1 should be the LRU, but K0 is locked.
    // If K1 is not referenced, it should be evicted.
    // In this specific test, let's assume we fill it up with 2 unique keys
    // and then attempt to add a 3rd.
    // K0 (locked), K1 (unlocked)
    ASSERT(cache->lru_oldest->key == key0);
    ASSERT(cache->lru_newest->key == key1);

    // K0 was accessed first, then K1. So K0 is LRU, but it's locked.
    // The implementation of find_an_old_unreferenced_node iterates from lru_oldest.
    // It should skip K0 and evict K1.
    err = cache->ops->get(cache, key2, &obj_ptr); // K1 should be evicted for K2
    ASSERT(err == OK); // Expect K1 to be evicted
    ASSERT(mock_ctx.written[key1] == false); // K1 was clean

    // Current cache should contain K0 (locked) and K2 (newest)
    ASSERT(cache->used_count == 2);
    ASSERT(cache->lru_oldest->key == key0); // K0 still oldest, but locked
    ASSERT(cache->lru_newest->key == key2); // K2 is newest

    // Now try to add another (key3) to a full cache where one is locked (K0)
    // and the other is not (K2). It should evict K2.
    err = cache->ops->get(cache, 3, &obj_ptr);
    ASSERT(err == OK); // K2 should be evicted for Key3
    ASSERT(cache->lru_oldest->key == key0); // K0 still oldest, but locked
    ASSERT(cache->lru_newest->key == 3); // Key3 is newest

    // Try to get another item, should fail as all are in use (K0 is locked, K2 is new)
    // No, here we need to test that if all items are referenced, it returns ERR_CONTAINER_FULL.
    cache->ops->destroy(cache);
    reset_mock_context(&mock_ctx);
    cache = create_test_cache(small_capacity, &mock_ctx); // Capacity 2
    ASSERT(cache != NULL);

    err = cache->ops->get(cache, key0, &obj_ptr); // Load K0
    ASSERT(err == OK);
    err = cache->ops->lock(cache, key0); // Lock K0
    ASSERT(err == OK);

    err = cache->ops->get(cache, key1, &obj_ptr); // Load K1
    ASSERT(err == OK);
    err = cache->ops->lock(cache, key1); // Lock K1
    ASSERT(err == OK);

    ASSERT(cache->used_count == 2);
    ASSERT(cache->references_sum == 2);

    // Cache is full and both items are locked.
    err = cache->ops->get(cache, key2, &obj_ptr); // Should fail to load K2
    ASSERT(err == ERR_CONTAINER_FULL);
    ASSERT(mock_ctx.load_count == 2); // K2 not loaded
    ASSERT(cache->used_count == 2);
    ASSERT(cache->references_sum == 2);

    cache->ops->unlock(cache, key0); // Unlock K0
    ASSERT(cache->references_sum == 1);
    err = cache->ops->get(cache, key2, &obj_ptr); // Now K0 can be evicted
    ASSERT(err == OK);
    ASSERT(mock_ctx.load_count == 3); // K2 loaded
    ASSERT(cache->used_count == 2); // K0 evicted, K2 added
    ASSERT(cache->references_sum == 1); // Only K1 is locked

    cache->ops->destroy(cache);
    log_info("test_lru_eviction PASSED.");
}

// Test case 8: Error Handling
static void test_error_handling() {
    log_info("Running test_error_handling...");
    mock_backend_context_t mock_ctx;
    reset_mock_context(&mock_ctx);
    mock_ctx.load_fail_key = (uint64_t)-1; // No fail by default
    mock_ctx.write_fail_key = (uint64_t)-1; // No fail by default

    backed_cache_t *cache = create_test_cache(TEST_CACHE_CAPACITY, &mock_ctx);
    ASSERT(cache != NULL);

    void *obj_ptr;
    error_t err;
    uint64_t key_load_fail = 0;
    uint64_t key_write_fail = 1;
    uint64_t key_non_existent = 99;
    uint8_t dummy_data[TEST_OBJ_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};

    // --- Test 1: Backend load error ---
    mock_ctx.load_fail_key = key_load_fail;
    err = cache->ops->get(cache, key_load_fail, &obj_ptr);
    ASSERT(err == ERR_NOT_FOUND); // Should propagate backend error as NOT_FOUND or similar
    ASSERT(mock_ctx.load_count == 0); // Should not have successfully loaded
    mock_ctx.load_fail_key = (uint64_t)-1; // Reset for next tests

    // --- Test 2: Backend write error on flush ---
    err = cache->ops->get(cache, key_write_fail, &obj_ptr);
    ASSERT(err == OK);
    err = cache->ops->mark_dirty(cache, key_write_fail);
    ASSERT(err == OK);
    ASSERT(cache->dirty_count == 1);

    mock_ctx.write_fail_key = key_write_fail;
    err = cache->ops->flush(cache, key_write_fail);
    ASSERT(err == ERR_IO_ERROR); // Should propagate backend write error
    ASSERT(cache->ops->is_dirty(cache, key_write_fail) == true); // Should still be dirty
    ASSERT(cache->dirty_count == 1);
    mock_ctx.write_fail_key = (uint64_t)-1; // Reset

    // --- Test 3: Backend write error during eviction (dirty item) ---
    // Fill cache with clean items first
    cache->ops->destroy(cache);
    reset_mock_context(&mock_ctx);
    mock_ctx.load_fail_key = (uint64_t)-1;
    mock_ctx.write_fail_key = (uint64_t)-1;
    cache = create_test_cache(2, &mock_ctx); // Small capacity for eviction
    ASSERT(cache != NULL);

    err = cache->ops->get(cache, 0, &obj_ptr);
    ASSERT(err == OK);
    err = cache->ops->get(cache, 1, &obj_ptr);
    ASSERT(err == OK);

    // Make oldest (key 0) dirty
    err = cache->ops->mark_dirty(cache, 0);
    ASSERT(err == OK);
    ASSERT(cache->dirty_count == 1);

    // Attempt to load a new item, forcing eviction of key 0 (dirty)
    mock_ctx.write_fail_key = 0;
    err = cache->ops->get(cache, 2, &obj_ptr); // This should try to evict and write key 0
    ASSERT(err == ERR_IO_ERROR); // Eviction write error
    mock_ctx.write_fail_key = (uint64_t)-1; // Reset

    // --- Test 4: Operations on non-existent keys (no implicit load) ---
    // Test is_locked
    ASSERT(cache->ops->is_locked(cache, key_non_existent) == false); // Should not find/load it
    
    // Test is_dirty
    ASSERT(cache->ops->is_dirty(cache, key_non_existent) == false); // Should not find/load it

    // Test lock/unlock on non-existent key (should implicitly load)
    // Already covered in test_lock_unlock_functionality, where it loads then locks.

    // Test mark_dirty on non-existent key (should return ERR_NOT_FOUND)
    err = cache->ops->mark_dirty(cache, key_non_existent);
    ASSERT(err == ERR_NOT_FOUND);

    // Test read/write/fill (these would normally load, but if ensure_node_in_cache fails, it's covered)
    // The current implementation of these calls `ensure_node_in_cache`, which will load the item.
    // So, if the key is truly non-existent, it means the `backend.load` failed or it was invalidated.
    // However, if the key is just not *yet* in cache, it will be loaded.
    // So, we test against a key that would *fail to load* to exercise these errors.
    mock_ctx.load_fail_key = key_non_existent;
    err = cache->ops->read(cache, key_non_existent, dummy_data);
    ASSERT(err == ERR_NOT_FOUND); // Propagates load failure
    err = cache->ops->write(cache, key_non_existent, dummy_data);
    ASSERT(err == ERR_NOT_FOUND); // Propagates load failure
    err = cache->ops->fill(cache, key_non_existent, 0);
    ASSERT(err == ERR_NOT_FOUND); // Propagates load failure
    mock_ctx.load_fail_key = (uint64_t)-1;

    cache->ops->destroy(cache);
    log_info("test_error_handling PASSED.");
}

void backed_cache_unit_tests() {
    log_info("Running backed_cache unit tests...");

    test_creation_destruction();
    test_get_load_functionality();
    test_dirty_tracking();
    test_flush_functionality();
    test_lock_unlock_functionality();
    test_lru_eviction();
    test_error_handling();

    // Add more test cases here
}


#endif // ENABLE_UNIT_TESTS
