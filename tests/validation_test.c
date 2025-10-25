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
    fake_block.magic = 0xFF; // 0xFF is an invalid magic number
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
    void *small_memory_bc = malloc(min_size - 1);
    Arena *small_static_arena_bc = arena_new_static(small_memory_bc, min_size - 1);
    ASSERT(small_static_arena_bc == NULL, "Static arena with size below minimum should fail");
    free(small_memory_bc);

    TEST_PHASE("Tail allocation leaving fragment smaller than block header");
    size_t arena_size_frag = 1024;
    Arena *arena_frag = arena_new_dynamic(arena_size_frag);
    ASSERT(arena_frag != NULL, "Arena creation for fragmentation test should succeed");

    // Calculate initial free size in tail
    size_t initial_tail_free = arena_frag->free_size_in_tail;
    ASSERT(initial_tail_free > sizeof(Block), "Initial tail should have space");

    // Calculate allocation size to leave a small fragment
    size_t fragment_size = sizeof(Block) / 2; // Must be > 0 and < sizeof(Block)
    if (fragment_size == 0) fragment_size = 1; // Ensure it's at least 1
    size_t alloc_size_frag = initial_tail_free - fragment_size;
    ASSERT(alloc_size_frag > 0, "Calculated alloc size must be positive");

    // Allocate the block
    void *block_frag = arena_alloc(arena_frag, alloc_size_frag);
    ASSERT(block_frag != NULL, "Allocation leaving small fragment should succeed");

    // Check that the 'else' branch in alloc_in_tail was taken
    ASSERT(arena_frag->free_size_in_tail == 0, "Tail free size should be 0 after small fragment alloc");

    arena_free(arena_frag);
}

void test_full_arena_allocation(void) {
    TEST_CASE("Allocation in Full Arena");

    // Create an arena with minimal valid size
    // Size = Arena metadata + one Block metadata + minimal usable buffer
    size_t min_valid_size = sizeof(Arena) + sizeof(Block) + MIN_BUFFER_SIZE;
    Arena *arena = arena_new_dynamic(min_valid_size);
    ASSERT(arena != NULL, "Arena creation with minimal size should succeed");
    #ifdef DEBUG
    print_fancy(arena, 100);
    print_arena(arena);
    #endif // DEBUG

    TEST_PHASE("Allocate block filling the entire initial tail");
    // Try to allocate exactly the minimum buffer size available
    void *first_block = arena_alloc(arena, MIN_BUFFER_SIZE - 5);
    ASSERT(first_block != NULL, "Allocation of the first block should succeed");
    #ifdef DEBUG
    print_fancy(arena, 100);
    print_arena(arena);
    #endif // DEBUG

    // At this point, the free list should be empty, and the tail should have 0 free space.
    ASSERT(arena->free_blocks == NULL, "Free block list should be empty after filling allocation");
    ASSERT(arena->free_size_in_tail == 0, "Free size in tail should be 0 after filling allocation");

    TEST_PHASE("Attempt allocation when no space is left");
    void *second_block = arena_alloc(arena, 1); // Attempt to allocate just one more byte
    ASSERT(second_block == NULL, "Allocation should fail when no space is left");

    arena_free(arena);
}

int main(void) {
    test_invalid_allocations();
    test_invalid_arena_creation();
    test_boundary_conditions();
    test_full_arena_allocation();
    
    print_test_summary();
    return tests_failed > 0 ? 1 : 0;
} 