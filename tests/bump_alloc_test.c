#define ARENA_IMPLEMENTATION
#include "arena.h"
#include "test_utils.h"

void test_bump_creation() {
    TEST_CASE("Bump Allocator Creation");

    TEST_PHASE("Create Bump Allocator within Arena");
    size_t arena_size = 1024;
    Arena *arena = arena_new_dynamic(arena_size);
    ASSERT(arena != NULL, "Arena should be created successfully");

    #ifdef DEBUG
    print_arena(arena);
    print_fancy(arena, 100);
    #endif

    size_t bump_size = 256;
    Bump *bump = bump_new(arena, bump_size);
    ASSERT(bump != NULL, "Bump allocator should be created successfully within the arena");
    #ifdef DEBUG
    print_arena(arena);
    print_fancy(arena, 100);
    #endif

    ASSERT(bump->capacity == bump_size, "Bump allocator capacity should match requested size");
    ASSERT(bump->arena == arena, "Bump allocator should reference the parent arena");
    ASSERT(bump->offset == sizeof(Bump), "Bump allocator offset should be initialized correctly");

    bump_free(bump);
    #ifdef DEBUG
    arena_free_block((char *)bump + sizeof(Bump));
    print_arena(arena);
    #endif

    bump = bump_new(arena, 0);
    ASSERT(bump == NULL, "Bump allocator creation with zero size should fail");
    if (bump) bump_free(bump);

    bump = bump_new(arena, -100);
    ASSERT(bump == NULL, "Bump allocator creation with negative size should fail");
    if (bump) bump_free(bump);

    bump = bump_new(NULL, 100);
    ASSERT(bump == NULL, "Bump allocator creation with NULL arena should fail");
    if (bump) bump_free(bump);

    bump = bump_new(arena, 2000); // Larger than arena size
    ASSERT(bump == NULL, "Bump allocator creation with size larger than arena should fail");
    if (bump) bump_free(bump);

    bump = bump_new(arena, arena_size - sizeof(Arena) - sizeof(Block));
    ASSERT(bump != NULL, "Bump allocator with size of all arena should be created successfully");

    arena_free(arena);
}

void test_bump_allocation() {
    TEST_CASE("Bump Allocator Allocation");

    size_t arena_size = 2048;
    Arena *arena = arena_new_dynamic(arena_size);
    ASSERT(arena != NULL, "Arena should be created successfully");

    size_t bump_size = 512;
    Bump *bump = bump_new(arena, bump_size);
    ASSERT(bump != NULL, "Bump allocator should be created successfully within the arena");

    TEST_PHASE("Allocate memory from Bump Allocator");
    size_t alloc_size1 = 100;
    void *ptr1 = bump_alloc(bump, alloc_size1);
    ASSERT(ptr1 != NULL, "First allocation from bump allocator should succeed");

    size_t alloc_size2 = 200;
    void *ptr2 = bump_alloc(bump, alloc_size2);
    ASSERT(ptr2 != NULL, "Second allocation from bump allocator should succeed");

    ASSERT((char *)ptr2 == (char *)ptr1 + alloc_size1, "Second allocation should be contiguous after first");

    size_t alloc_size3 = 300; // Exceeds remaining space
    void *ptr3 = bump_alloc(bump, alloc_size3);
    ASSERT(ptr3 == NULL, "Allocation exceeding bump allocator capacity should fail");
    
    TEST_PHASE("Reset Bump Allocator");
    bump_reset(bump);
    ASSERT(bump->offset == sizeof(Bump), "Bump allocator offset should be reset correctly");
    ASSERT(bump->capacity == bump_size, "Bump allocator capacity should remain unchanged after reset");

    TEST_PHASE("Allocate aligned memory from Bump Allocator");
    size_t alloc_size4 = 50;
    size_t alignment4 = 3;
    void *ptr4 = bump_alloc_aligned(bump, alloc_size4, alignment4);
    ASSERT(ptr4 == NULL, "Aligned allocation with non-power-of-two alignment should fail");

    size_t alloc_size5 = 50;
    size_t alignment5 = 64;
    void *ptr5 = bump_alloc_aligned(bump, alloc_size5, alignment5);
    ASSERT(ptr5 != NULL, "Aligned allocation from bump allocator should succeed");
    ASSERT(((uintptr_t)ptr5 % alignment5) == 0, "Allocated pointer should be correctly aligned");

    size_t alloc_size6 = 450; // Exceeds remaining space
    void *ptr6 = bump_alloc_aligned(bump, alloc_size6, alignment5);
    ASSERT(ptr6 == NULL, "Aligned allocation exceeding bump allocator capacity should fail");
    
    bump_reset(bump);
    
    size_t alloc_size7 = 0;
    void *ptr7 = bump_alloc_aligned(bump, alloc_size7, alignment5);
    ASSERT(ptr7 == NULL, "Aligned allocation with zero size should fail");

    size_t alloc_size8 = 100;
    size_t alignment8 = -1;
    void *ptr8 = bump_alloc_aligned(bump, alloc_size8, alignment8);
    ASSERT(ptr8 == NULL, "Aligned allocation with negative alignment should fail");

    size_t alloc_size9 = bump_size;
    size_t alignment9 = 16;
    void *ptr9 = bump_alloc_aligned(bump, alloc_size9, alignment9);
    ASSERT(ptr9 == NULL, "Aligned allocation that exactly matches bump capacity should fail");

    bump_reset(bump);
    void *ptr10 = bump_alloc(bump, (ssize_t)SIZE_MAX); // Cast for warning suppression if needed
    ASSERT(ptr10 == NULL, "Huge allocation must fail gracefully");

    TEST_PHASE("Free Bump Allocator");
    bump_free(bump);
    arena_free(arena);
}

void test_bump_hard_usage() {
    TEST_PHASE("Bump Integrity / Hard Usage");
    Arena *arena = arena_new_dynamic(5000);
    Bump *bump = bump_new(arena, 4096);
    const int NUM_ALLOCS = 100;
    void *ptrs[NUM_ALLOCS];
    size_t sizes[NUM_ALLOCS];
    
    for(int i=0; i<NUM_ALLOCS; i++) {
        sizes[i] = 10 + (i % 20);
        ptrs[i] = bump_alloc(bump, sizes[i]);
        
        ASSERT_QUIET(ptrs[i] != NULL, "Stress test allocation");
        fill_memory_pattern(ptrs[i], sizes[i], i);
    }
    
    for(int i=0; i<NUM_ALLOCS; i++) {
        ASSERT_QUIET(verify_memory_pattern(ptrs[i], sizes[i], i), "Pattern verification failed for block");
    }
    
    check_pointers_integrity(ptrs, sizes, NUM_ALLOCS);
    
    bump_free(bump);
    arena_free(arena);
}

int main(void) {
    test_bump_creation();
    test_bump_allocation();
    test_bump_hard_usage();

    // Print test summary
    print_test_summary();
    
    // Return non-zero exit code if any tests failed
    return tests_failed > 0 ? 1 : 0;
}
