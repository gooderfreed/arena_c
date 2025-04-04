#define ARENA_IMPLEMENTATION
#include "arena.h"
#include "test_utils.h"

void test_invalid_allocations(void) {
    TEST_CASE("Invalid Allocation Scenarios");

    // Create an arena
    Arena *arena = arena_new_dynamic(1024);
    ASSERT(arena != NULL, "Arena creation should succeed");

    TEST_PHASE("Zero size allocation");
    void *zero_size = arena_alloc(arena, 0);
    ASSERT(zero_size == NULL, "Zero size allocation should return NULL");

    TEST_PHASE("Negative size allocation");
    void *negative_size = arena_alloc(arena, -1);
    ASSERT(negative_size == NULL, "Negative size allocation should return NULL");

    TEST_PHASE("NULL arena allocation");
    void *null_arena = arena_alloc(NULL, 32);
    ASSERT(null_arena == NULL, "Allocation with NULL arena should return NULL");

    TEST_PHASE("Free NULL pointer");
    arena_free_block(NULL); // Should not crash
    ASSERT(true, "Free NULL pointer should not crash");

    TEST_PHASE("Free invalid pointer");
    int invalid_ptr = 42;
    arena_free_block((void*)&invalid_ptr); // Should not crash
    ASSERT(true, "Free invalid pointer should not crash");
    
    TEST_PHASE("Free pointer from different arena");
    Arena *another_arena = arena_new_dynamic(1024);
    void *ptr = arena_alloc(another_arena, 32);
    arena_free_block(ptr); // Should not crash
    arena_free(another_arena);

    TEST_PHASE("Free already freed pointer");
    void *ptr2 = arena_alloc(arena, 32);
    arena_free_block(ptr2);
    arena_free_block(ptr2); // Should not crash

    TEST_PHASE("Allocation larger than arena size");
    void *huge_allocation = arena_alloc(arena, 2048);
    ASSERT(huge_allocation == NULL, "Allocation larger than arena size should fail");

    arena_free(arena);
}

void test_invalid_arena_creation(void) {
    TEST_CASE("Invalid Arena Creation Scenarios");

    TEST_PHASE("Zero size arena");
    Arena *zero_size_arena = arena_new_dynamic(0);
    ASSERT(zero_size_arena == NULL, "Zero size arena creation should fail");

    TEST_PHASE("Negative size arena");
    Arena *negative_size_arena = arena_new_dynamic(-1);
    ASSERT(negative_size_arena == NULL, "Negative size arena creation should fail");

    TEST_PHASE("Free NULL arena");
    arena_free(NULL); // Should not crash
}

int main(void) {
    test_invalid_allocations();
    test_invalid_arena_creation();
    
    print_test_summary();
    return tests_failed > 0 ? 1 : 0;
} 