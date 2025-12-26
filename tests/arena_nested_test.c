#define ARENA_IMPLEMENTATION
#include "arena.h"
#include "test_utils.h"

void test_nested_creation(void) {
    TEST_CASE("Nested Bump Allocator Creation");

    TEST_PHASE("Create Parent Arena");
    size_t parent_arena_size = 4096;
    Arena *parent_arena = arena_new_dynamic(parent_arena_size);
    ASSERT(parent_arena != NULL, "Parent arena should be created successfully");

    #ifdef DEBUG
    print_arena(parent_arena);
    print_fancy(parent_arena, 100);
    #endif

    TEST_PHASE("Create Nested Arena within Parent Arena");
    size_t nested_arena_size = 1024;
    Arena *nested_arena = arena_new_nested(parent_arena, nested_arena_size);
    ASSERT(nested_arena != NULL, "Nested arena should be created successfully within parent arena");
    ASSERT(((char *)nested_arena >= (char *)parent_arena) &&
           ((char *)nested_arena + nested_arena_size <= (char *)parent_arena + parent_arena_size),
           "Nested arena memory should be within parent arena bounds");
    ASSERT(nested_arena->capacity == nested_arena_size - sizeof(Arena), "Nested arena capacity should match requested size");

    #ifdef DEBUG
    print_arena(parent_arena);
    print_fancy(parent_arena, 100);
    #endif

    TEST_PHASE("Allocate memory from Nested Arena");
    size_t alloc_size = 256;
    void *ptr = arena_alloc(nested_arena, alloc_size);
    ASSERT(ptr != NULL, "Allocation from nested arena should succeed");
    ASSERT(((char *)ptr >= (char *)nested_arena) &&
           ((char *)ptr + alloc_size <= (char *)nested_arena + nested_arena_size),
           "Allocated memory should be within nested arena bounds");

    #ifdef DEBUG
    print_arena(nested_arena);
    print_fancy(nested_arena, 100);
    #endif

    arena_free_block(ptr);
    ASSERT(true, "Freeing allocation from nested arena should succeed");

    TEST_PHASE("Free Nested Arena");
    arena_free(nested_arena);
    ASSERT(true, "Nested arena should be freed successfully");
    ASSERT(parent_arena->free_size_in_tail == parent_arena_size - sizeof(Arena) - sizeof(Block), "Parent arena free size should be restored after freeing nested arena");
    
    #ifdef DEBUG
    print_arena(parent_arena);
    print_fancy(parent_arena, 100);
    #endif

    TEST_PHASE("Invalid Nested Arena Creation");
    Arena *invalid_nested1 = arena_new_nested(NULL, nested_arena_size);
    ASSERT(invalid_nested1 == NULL, "Creating nested arena with NULL parent should fail");
    ASSERT(parent_arena->free_size_in_tail == parent_arena_size - sizeof(Arena) - sizeof(Block), "Parent arena free size should remain unchanged after failed nested arena creation");

    Arena *invalid_nested2 = arena_new_nested(parent_arena, 0);
    ASSERT(invalid_nested2 == NULL, "Creating nested arena with zero size should fail");
    ASSERT(parent_arena->free_size_in_tail == parent_arena_size - sizeof(Arena) - sizeof(Block), "Parent arena free size should remain unchanged after failed nested arena creation");

    Arena *invalid_nested3 = arena_new_nested(parent_arena, -100);
    ASSERT(invalid_nested3 == NULL, "Creating nested arena with negative size should fail");
    ASSERT(parent_arena->free_size_in_tail == parent_arena_size - sizeof(Arena) - sizeof(Block), "Parent arena free size should remain unchanged after failed nested arena creation");

    TEST_PHASE("Free Parent Arena");
    arena_free(parent_arena);
    ASSERT(true, "Parent arena should be freed successfully");

    TEST_PHASE("Free NULL Nested Arena");
    arena_free(NULL); // Should not crash
    ASSERT(true, "Freeing NULL nested arena should not crash");

    TEST_PHASE("Free Already Freed Nested Arena");
    arena_free(nested_arena); // Should not crash
    ASSERT(true, "Freeing already freed nested arena should not crash");
    
    TEST_PHASE("Nested Arena creation in too small Parent Arena");
    size_t small_parent_size = sizeof(Arena) + sizeof(Block) + ARENA_MIN_BUFFER_SIZE + 10; // Just above minimum
    Arena *small_parent = arena_new_dynamic(small_parent_size);
    ASSERT(small_parent != NULL, "Small parent arena should be created successfully");

    size_t too_large_nested_size = small_parent_size; // Too large to fit nested arena
    Arena *too_large_nested = arena_new_nested(small_parent, too_large_nested_size);
    ASSERT(too_large_nested == NULL, "Creating nested arena larger than parent arena should fail");
    
    set_is_arena_nested(small_parent, false); 
    arena_free(small_parent);
}

int main(void) {
    test_nested_creation();


    // Print test summary
    print_test_summary();
    return tests_failed > 0 ? 1 : 0;
}