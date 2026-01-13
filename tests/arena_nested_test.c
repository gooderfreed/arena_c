#define ARENA_IMPLEMENTATION
#include "arena.h"
#include "test_utils.h"

void test_nested_creation(void) {
    TEST_PHASE("Nested Arena Creation");

    TEST_CASE("Create Parent Arena");
    size_t parent_arena_size = 4096;
    Arena *parent_arena = arena_new_dynamic(parent_arena_size);
    size_t parent_arena_size_in_tail = free_size_in_tail(parent_arena);
    ASSERT(parent_arena != NULL, "Parent arena should be created successfully");

    #ifdef DEBUG
    print_arena(parent_arena);
    print_fancy(parent_arena, 100);
    #endif

    TEST_CASE("Create Nested Arena within Parent Arena");
    size_t nested_arena_size = 1024;
    Arena *nested_arena = arena_new_nested(parent_arena, nested_arena_size);
    ASSERT(nested_arena != NULL, "Nested arena should be created successfully within parent arena");
    ASSERT(((char *)nested_arena >= (char *)parent_arena) &&
           ((char *)nested_arena + nested_arena_size <= (char *)parent_arena + parent_arena_size),
           "Nested arena memory should be within parent arena bounds");
    ASSERT(arena_get_capacity(nested_arena) == nested_arena_size, "Nested arena capacity should match requested size");

    #ifdef DEBUG
    print_arena(parent_arena);
    print_fancy(parent_arena, 100);
    #endif

    TEST_CASE("Allocate memory from Nested Arena");
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

    TEST_CASE("Free Nested Arena");
    arena_free(nested_arena);
    ASSERT(true, "Nested arena should be freed successfully");
    ASSERT(free_size_in_tail(parent_arena) == parent_arena_size_in_tail, "Parent arena free size should be restored after freeing nested arena");
    
    #ifdef DEBUG
    print_arena(parent_arena);
    print_fancy(parent_arena, 100);
    #endif

    TEST_CASE("Invalid Nested Arena Creation");
    Arena *invalid_nested1 = arena_new_nested(NULL, nested_arena_size);
    ASSERT(invalid_nested1 == NULL, "Creating nested arena with NULL parent should fail");
    ASSERT(free_size_in_tail(parent_arena) == parent_arena_size_in_tail, "Parent arena free size should remain unchanged after failed nested arena creation");

    Arena *invalid_nested2 = arena_new_nested(parent_arena, 0);
    ASSERT(invalid_nested2 == NULL, "Creating nested arena with zero size should fail");
    ASSERT(free_size_in_tail(parent_arena) == parent_arena_size_in_tail, "Parent arena free size should remain unchanged after failed nested arena creation");

    Arena *invalid_nested3 = arena_new_nested(parent_arena, -100);
    ASSERT(invalid_nested3 == NULL, "Creating nested arena with negative size should fail");
    ASSERT(free_size_in_tail(parent_arena) == parent_arena_size_in_tail, "Parent arena free size should remain unchanged after failed nested arena creation");

    TEST_CASE("Free Parent Arena");
    arena_free(parent_arena);
    ASSERT(true, "Parent arena should be freed successfully");

    TEST_CASE("Free NULL Nested Arena");
    arena_free(NULL); // Should not crash
    ASSERT(true, "Freeing NULL nested arena should not crash");

    TEST_CASE("Free Already Freed Nested Arena");
    arena_free(nested_arena); // Should not crash
    ASSERT(true, "Freeing already freed nested arena should not crash");
    
    TEST_CASE("Nested Arena creation in too small Parent Arena");
    size_t small_parent_size = sizeof(Arena) + sizeof(Block) + ARENA_MIN_BUFFER_SIZE + 10; // Just above minimum
    Arena *small_parent = arena_new_dynamic(small_parent_size);
    ASSERT(small_parent != NULL, "Small parent arena should be created successfully");

    size_t too_large_nested_size = small_parent_size; // Too large to fit nested arena
    Arena *too_large_nested = arena_new_nested(small_parent, too_large_nested_size);
    ASSERT(too_large_nested == NULL, "Creating nested arena larger than parent arena should fail");
    
    arena_set_is_nested(small_parent, false); 
    arena_free(small_parent);
}

void test_nested_freeing(void) {
    TEST_PHASE("Nested Arena Freeing");

    TEST_CASE("Freeing Nested Arena through Parent Arena");
    size_t parent_arena_size = 8192;
    Arena *parent_arena = arena_new_dynamic(parent_arena_size);
    ASSERT(parent_arena != NULL, "Parent arena should be created successfully");
    
    size_t parent_free_before = free_size_in_tail(parent_arena);

    size_t nested_arena_size = 2048;
    Arena *nested_arena = arena_new_nested(parent_arena, nested_arena_size);
    ASSERT(nested_arena != NULL, "Nested arena should be created successfully within parent arena");


    arena_free(nested_arena); // Free nested arena through parent
    ASSERT(true, "Freeing nested arena through parent should succeed");
    ASSERT(free_size_in_tail(parent_arena) == parent_free_before, "Parent arena free size should be restored after freeing nested arena");


    void *ptr = arena_alloc(parent_arena, 512);
    ASSERT(ptr != NULL, "Allocation from parent arena after freeing nested arena should succeed");
    Arena *check_nested = arena_new_nested(parent_arena, nested_arena_size);
    ASSERT(check_nested != NULL, "Should be able to create new nested arena after freeing previous nested arena");
    arena_free(check_nested);

    
    Arena *another_nested = arena_new_nested(parent_arena, nested_arena_size);
    ASSERT(another_nested != NULL, "Another nested arena should be created successfully within parent arena");
    arena_free_block(ptr);
    ASSERT(true, "Freeing allocation from parent arena should succeed");
    arena_free(another_nested);
    ASSERT(true, "Freeing another nested arena should succeed");


    arena_free(parent_arena);
    ASSERT(true, "Parent arena should be freed successfully");
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0); 

    test_nested_creation();
    test_nested_freeing();

    // Print test summary
    print_test_summary();
    return tests_failed > 0 ? 1 : 0;
}