#define ARENA_IMPLEMENTATION
#include "arena.h"
#include "test_utils.h"
#include <limits.h>
#include <stdint.h>

#include <limits.h> // This is guaranteed to exist by the C standard

#ifndef SSIZE_MAX
#define SSIZE_MAX (SIZE_MAX / 2)
#endif

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
    fake_block.magic = (void *)0xFF; // 0xFF is an invalid magic number
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
    #if SIZE_MAX > 0xFFFFFFFF
        Arena *large_size_arena = arena_new_dynamic(SSIZE_MAX);
        ASSERT(large_size_arena == NULL, "Very large size arena creation should fail on 64-bit systems");
    #else
        printf("[INFO] Skipping SSIZE_MAX allocation test on 32-bit system.\n");
    #endif
    
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

void test_static_arena_creation(void) {
    TEST_CASE("Static Arena Creation");

    TEST_PHASE("Valid static arena creation");
    size_t static_arena_size = 2048;
    void *static_memory = malloc(static_arena_size);
    Arena *static_arena = arena_new_static(static_memory, static_arena_size);
    ASSERT(static_arena != NULL, "Static arena creation with valid memory should succeed");

    TEST_PHASE("Allocation from static arena");
    void *alloc1 = arena_alloc(static_arena, 512);
    ASSERT(alloc1 != NULL, "Allocation from static arena should succeed");

    void *alloc2 = arena_alloc(static_arena, 1024);
    ASSERT(alloc2 != NULL, "Second allocation from static arena should succeed");

    void *alloc3 = arena_alloc(static_arena, 1024); // This should fail
    ASSERT(alloc3 == NULL, "Allocation exceeding static arena capacity should fail");

    arena_free(static_arena);
    free(static_memory);
}

void test_freeing_invalid_blocks(void) {
    TEST_CASE("Freeing Invalid Blocks");

    // Create an arena
    Arena *arena = arena_new_dynamic(1024);
    ASSERT(arena != NULL, "Arena creation should succeed");

    TEST_PHASE("Freeing a pointer not allocated by the arena");
    int stack_var = 42;
    arena_free_block(&stack_var); // Should not crash
    ASSERT(true, "Freeing stack variable should not crash");

    TEST_PHASE("Freeing a pointer with valid magic number");
    Block fake_block = {0};
    fake_block.magic = (void *)0xDEAFBEEF; // Valid magic number
    void *fake_data = (char*)&fake_block + sizeof(Block);
    arena_free_block(fake_data); // Should not crash
    ASSERT(true, "Freeing block with valid magic number should not crash");

    TEST_PHASE("Freeing a pointer from a different arena");
    Arena *another_arena = arena_new_dynamic(1024);
    void *ptr = arena_alloc(another_arena, 32);
    arena_free_block(ptr); // Should not crash
    ASSERT(true, "Freeing block from different arena should not crash");
    arena_free(another_arena);

    arena_free(arena);
}

void test_calloc() {
    TEST_CASE("Arena Calloc Functionality");

    // Create an arena
    Arena *arena = arena_new_dynamic(1024);
    ASSERT(arena != NULL, "Arena creation should succeed");
    
    #ifdef DEBUG
    print_fancy(arena, 100);
    print_arena(arena);
    #endif

    TEST_PHASE("Calloc a block and verify zero-initialization");
    size_t num_elements = 10;
    size_t element_size = sizeof(int);
    int *array = (int *)arena_calloc(arena, num_elements, element_size);
    ASSERT(array != NULL, "Calloc should succeed");

    // Verify all elements are zero
    bool all_zero = true;
    for (size_t i = 0; i < num_elements; i++) {
        if (array[i] != 0) {
            all_zero = false;
            break;
        }
    }
    ASSERT(all_zero, "All elements in calloced array should be zero");

    #ifdef DEBUG
    print_fancy(arena, 100);
    print_arena(arena);
    #endif

    arena_free_block(array);
    ASSERT(true, "Freeing calloced block should succeed");

    #ifdef DEBUG
    print_fancy(arena, 100);
    print_arena(arena);
    #endif

    int *overflow_array = (int *)arena_calloc(arena, SIZE_MAX, sizeof(int));
    ASSERT(overflow_array == NULL, "Calloc with overflow should return NULL");

    #ifdef DEBUG
    print_fancy(arena, 100);
    print_arena(arena);
    #endif

    int *null_arena_array = (int *)arena_calloc(NULL, 10, sizeof(int));
    ASSERT(null_arena_array == NULL, "Calloc with NULL arena should return NULL");

    #ifdef DEBUG
    print_fancy(arena, 100);
    print_arena(arena);
    #endif

    int *zero_nmemb_array = (int *)arena_calloc(arena, 0, sizeof(int));
    ASSERT(zero_nmemb_array == NULL, "Calloc with zero nmemb should return NULL");

    #ifdef DEBUG
    print_fancy(arena, 100);
    print_arena(arena);
    #endif

    arena_free(arena);
    arena = arena_new_dynamic(1000);

    void *almost_full = arena_alloc(arena, 751); // Fill up the arena
    ASSERT(almost_full != NULL, "Allocation to nearly fill arena should succeed");

    #ifdef DEBUG
    print_fancy(arena, 100);
    print_arena(arena);
    #endif

    void *tail = arena_alloc(arena, 152);
    ASSERT(tail != NULL, "Allocation to fill arena should succeed");

    #ifdef DEBUG
    print_fancy(arena, 100);
    print_arena(arena);
    #endif

    arena_free(arena);
}

int main(void) {
    test_invalid_allocations();
    test_invalid_arena_creation();
    test_boundary_conditions();
    test_full_arena_allocation();
    test_static_arena_creation();
    test_freeing_invalid_blocks();
    test_calloc();
    
    print_test_summary();
    return tests_failed > 0 ? 1 : 0;
} 