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

    bump = bump_new(arena, 10);
    ASSERT(bump == NULL, "Bump creation with too small positive size should fail");
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

    bump_free(bump);

    bump_free(NULL); // Should not crash
    ASSERT(true, "Freeing NULL bump allocator should not crash");

    bump_reset(NULL); // Should not crash
    ASSERT(true, "Resetting NULL bump allocator should not crash");

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

    bump_alloc(NULL, 100); // Should not crash
    ASSERT(true, "Allocating from NULL bump allocator should not crash");

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

#define BLOCK_FROM_DATA(ptr) ((Block *)((char *)(ptr) - sizeof(Block)))

void test_bump_trim(void) {
    TEST_CASE("Bump Trim Scenarios");

    // ---------------------------------------------------------
    TEST_PHASE("1. Trim NULL");
    bump_trim(NULL);
    ASSERT(1, "bump_trim(NULL) should not crash");

    // ---------------------------------------------------------
    TEST_PHASE("2. Trim when not enough space (No-op)");
    {
        Arena *arena = arena_new_dynamic(4096);
        Bump *bump = bump_new(arena, 100); 
        printf("capacity: %zu\n", bump->capacity);

        bump_alloc(bump, 90);
        printf("free space after alloc: %zu\n", bump->capacity - bump->offset + sizeof(Bump));
        
        size_t old_capacity = bump->capacity;
        bump_trim(bump);
        
        ASSERT(bump->capacity == old_capacity, "Capacity should not change if remaining space is too small");
        
        arena_free(arena);
    }

    // ---------------------------------------------------------
    TEST_PHASE("3. Trim with plenty of space (Tail Merge Scenario)");
    {
        Arena *arena = arena_new_dynamic(2048);
        Bump *bump = bump_new(arena, 1024);
        #ifdef DEBUG
        print_arena(bump->arena);
        print_fancy(bump->arena, 101);
        #endif

        void *ptr = bump_alloc(bump, 64);
        void *old_tail = arena->tail;
        
        bump_trim(bump);

        #ifdef DEBUG
        print_arena(bump->arena);
        print_fancy(bump->arena, 101);
        #endif
        
        size_t aligned_ptr = align_up((uintptr_t)ptr + 64, ARENA_DEFAULT_ALIGNMENT);
        size_t expected_cap = aligned_ptr - (uintptr_t)bump - sizeof(Bump);
        
        ASSERT(bump->capacity == expected_cap, "Capacity should shrink to fit used data");
        
        Block *arena_tail = get_pointer(arena->tail);
        ASSERT((void*)arena_tail < (void*)old_tail, "Arena tail should point to the trimmed bump");
        
        arena_free(arena);
    }

    // ---------------------------------------------------------
    TEST_PHASE("4. Trim when space is JUST enough (Boundary check)");
    {
        Arena *arena = arena_new_dynamic(2048);

        Bump *bump = bump_new(arena, 64);
        
        ssize_t alloc_size = 64 - sizeof(Block) - ARENA_DEFAULT_ALIGNMENT;

        bump_alloc(bump, alloc_size);
        
        bump_trim(bump);
        
        #ifdef DEBUG
        print_arena(bump->arena);
        print_fancy(bump->arena, 101);
        #endif

        ASSERT(bump->capacity == alloc_size, "Trim should work on exact boundary condition");
        
        arena_free(arena);
    }

    // ---------------------------------------------------------
    TEST_PHASE("5. Trim when right neighbor is OCCUPIED");
    {
        Arena *arena = arena_new_dynamic(2048);
        
        // [Bump (1024)] -> [Block C (Occupied)]
        Bump *bump = bump_new(arena, 1024);
        void *data_c = arena_alloc(arena, 64);
        Block *block_c = BLOCK_FROM_DATA(data_c);
        
        bump_alloc(bump, 64);
        
        bump_trim(bump);

        #ifdef DEBUG
        print_arena(bump->arena);
        print_fancy(bump->arena, 101);
        #endif
        
        Block *new_free = get_pointer(block_c->prev);
        ASSERT(new_free != (Block*)bump, "New block should be inserted between Bump and C");
        ASSERT(get_is_free(new_free), "Inserted block should be free");
        ASSERT(new_free->size > 0, "Inserted block should have size");
        
        ASSERT(get_pointer(new_free->prev) == (Block*)bump, "New free block should point back to bump");
        
        arena_free(arena);
    }

    // ---------------------------------------------------------
    TEST_PHASE("6. Trim when right neighbor is FREE (Merge Right)");
    {
        Arena *arena = arena_new_dynamic(2048);
        
        // [Bump (1024)] -> [Block B (Free)] -> [Block C (Occupied)]
        Bump *bump = bump_new(arena, 1024);
        void *data_b = arena_alloc(arena, 256);
        void *data_c = arena_alloc(arena, 64);
        
        arena_free_block(data_b);
        Block *block_b = BLOCK_FROM_DATA(data_b);
        size_t old_b_size = block_b->size;
        
        bump_alloc(bump, 64);
        
        bump_trim(bump);

        #ifdef DEBUG
        print_arena(bump->arena);
        print_fancy(bump->arena, 101);
        #endif
        
        Block *next_after_bump = next_block(arena, (Block*)bump);
        ASSERT(get_is_free(next_after_bump), "Next block should be free");
        ASSERT(next_after_bump->size > old_b_size, "Free block should have grown due to merge");
        
        arena_free_block(data_c);
        arena_free(arena);
    }

    // ---------------------------------------------------------
    TEST_PHASE("7. Trim when space is large (Offset Alignment check)");
    {
        Arena *arena = arena_new_dynamic(2048);
        Bump *bump = bump_new(arena, 100);
        
        bump_alloc(bump, 1);
        
        bump_trim(bump);

        #ifdef DEBUG
        print_arena(bump->arena);
        print_fancy(bump->arena, 101);
        #endif

        ASSERT(bump->capacity == 16, "Trim should align capacity up");
        
        arena_free(arena);
    }
}

int main(void) {
    test_bump_creation();
    test_bump_allocation();
    test_bump_hard_usage();
    test_bump_trim();

    // Print test summary
    print_test_summary();
    
    // Return non-zero exit code if any tests failed
    return tests_failed > 0 ? 1 : 0;
}
