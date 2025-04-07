#define ARENA_IMPLEMENTATION
#include "arena.h"
#include "test_utils.h"
#include <limits.h>
#include <stdint.h>

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
    Block fake_block = {0};
    fake_block.flags.raw = 0xFF; // 0xFF is an invalid magic number
    void *fake_data = (char*)&fake_block + sizeof(Block);
    arena_free_block(fake_data); // Should not crash
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
    ASSERT(true, "Free already freed pointer should not crash");

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

    TEST_PHASE("Very large size arena");
    Arena *large_size_arena = arena_new_dynamic(INT64_MAX);
    ASSERT(large_size_arena == NULL, "Very large size arena creation should fail");

    TEST_PHASE("NULL memory for static arena");
    Arena *null_memory_arena = arena_new_static(NULL, 1024);
    ASSERT(null_memory_arena == NULL, "Static arena with NULL memory should fail");

    TEST_PHASE("Negative size for static arena");
    void *mem = malloc(1024);
    Arena *negative_static_arena = arena_new_static(mem, -1);
    ASSERT(negative_static_arena == NULL, "Static arena with negative size should fail");
    free(mem);

    TEST_PHASE("Free NULL arena");
    arena_free(NULL); // Should not crash
    ASSERT(true, "Free NULL arena should not crash");

    TEST_PHASE("Reset NULL arena");
    arena_reset(NULL); // Should not crash
    ASSERT(true, "Reset NULL arena should not crash");
}

void test_boundary_conditions(void) {
    TEST_CASE("Boundary Conditions");

    TEST_PHASE("Arena size just above minimum");
    size_t min_size = sizeof(Arena) + sizeof(Block) + MIN_BUFFER_SIZE;
    Arena *min_size_arena = arena_new_dynamic(min_size);
    ASSERT(min_size_arena != NULL, "Arena with minimum valid size should succeed");
    arena_free(min_size_arena);

    TEST_PHASE("Arena size just below minimum");
    Arena *below_min_arena = arena_new_dynamic(min_size - 1);
    ASSERT(below_min_arena == NULL, "Arena with size below minimum should fail");

    TEST_PHASE("Static arena with minimum size");
    void *min_memory = malloc(min_size);
    Arena *min_static_arena = arena_new_static(min_memory, min_size);
    ASSERT(min_static_arena != NULL, "Static arena with minimum valid size should succeed");
    free(min_memory);

    TEST_PHASE("Static arena with size below minimum");
    void *small_memory = malloc(min_size - 1);
    Arena *small_static_arena = arena_new_static(small_memory, min_size - 1);
    ASSERT(small_static_arena == NULL, "Static arena with size below minimum should fail");
    free(small_memory);
}

int main(void) {
    test_invalid_allocations();
    test_invalid_arena_creation();
    test_boundary_conditions();
    
    print_test_summary();
    return tests_failed > 0 ? 1 : 0;
} 