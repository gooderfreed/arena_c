#define ARENA_IMPLEMENTATION
#include "arena.h"
#include "test_utils.h"

#define MAX_OBJECTS 300
#define ARENA_SIZE (10 * 1024)

/*
 * Test complex allocation pattern
 * Imitates a real scenario of dynamic object graph management
 */
void test_complex_allocation_pattern(void) {
    TEST_CASE("Complex Allocation Pattern");

    // Create an arena
    Arena *arena = arena_new_dynamic(ARENA_SIZE);
    ASSERT(arena != NULL, "Arena creation should succeed");

    void *objects[MAX_OBJECTS] = {0};  // Allocated objects
    size_t sizes[MAX_OBJECTS] = {0};   // Size of each allocated object
    int allocated = 0;
    int alloc_errors = 0;
    int pattern_errors = 0;
    
    TEST_PHASE("Initial allocations");
    // Allocate objects of various sizes
    for (int i = 0; i < 50; i++) {
        size_t size = 20 + (i * 7) % 180;
        objects[allocated] = arena_alloc(arena, size);
        
        if (objects[allocated]) {
            sizes[allocated] = size;
            
            // Fill the memory with a test pattern to verify later
            fill_memory_pattern(objects[allocated], size, i);
            
            if (!verify_memory_pattern(objects[allocated], size, i)) {
                pattern_errors++;
            }
                   
            allocated++;
        } else {
            alloc_errors++;
        }
    }
    
    // Summary report instead of spamming asserts
    ASSERT(allocated > 0, "Should successfully allocate some objects");
    ASSERT(pattern_errors == 0, "All memory patterns should be valid");
    
    #ifdef DEBUG
    printf("Allocated %d objects of various sizes (%d allocation failures)\n", allocated, alloc_errors);
    #endif // DEBUG
    
    // Check that allocated objects don't overlap
    check_pointers_integrity(objects, sizes, allocated);
    
    #ifdef DEBUG
    print_fancy(arena, 100);
    #endif // DEBUG
    
    TEST_PHASE("Free every third object");
    // Free every third object
    int freed_count = 0;
    
    for (int i = 0; i < allocated; i += 3) {
        if (objects[i]) {
            arena_free_block(objects[i]);
            objects[i] = NULL;
            sizes[i] = 0;
            freed_count++;
        }
    }
    
    ASSERT(freed_count > 0, "Should successfully free some objects");
    
    #ifdef DEBUG
    printf("Freed %d objects\n", freed_count);
    print_fancy(arena, 100);
    #endif // DEBUG

    TEST_PHASE("Allocate small objects");
    // Try to allocate small objects
    int small_alloc_count = 0;
    pattern_errors = 0;
    
    for (int i = 0; i < 20; i++) {
        size_t size = 25 + (i * 3) % 15;
        void *ptr = arena_alloc(arena, size);
        
        if (ptr) {
            objects[allocated] = ptr;
            sizes[allocated] = size;
            
            // Fill with a pattern
            fill_memory_pattern(ptr, size, 100 + i);
            
            if (!verify_memory_pattern(ptr, size, 100 + i)) {
                pattern_errors++;
            }
                   
            allocated++;
            small_alloc_count++;
        }
    }
    
    ASSERT(small_alloc_count > 0, "Should successfully allocate some small objects");
    ASSERT(pattern_errors == 0, "All small objects memory patterns should be valid");
    
    #ifdef DEBUG
    printf("Allocated %d small objects\n", small_alloc_count);
    #endif // DEBUG
    
    // Check integrity
    check_pointers_integrity(objects, sizes, allocated);
    
    #ifdef DEBUG
    print_fancy(arena, 100);
    #endif // DEBUG
    
    TEST_PHASE("Allocate large objects");
    // Try to allocate large objects
    int large_alloc_count = 0;
    pattern_errors = 0;
    
    for (int i = 0; i < 10; i++) {
        size_t size = 150 + (i * 17) % 100;
        void *ptr = arena_alloc(arena, size);
        
        if (ptr) {
            objects[allocated] = ptr;
            sizes[allocated] = size;
            
            // Fill with a pattern
            fill_memory_pattern(ptr, size, 200 + i);
            
            if (!verify_memory_pattern(ptr, size, 200 + i)) {
                pattern_errors++;
            }
                   
            allocated++;
            large_alloc_count++;
        }
    }
    
    ASSERT(large_alloc_count > 0, "Should successfully allocate some large objects");
    ASSERT(pattern_errors == 0, "All large objects memory patterns should be valid");
    
    #ifdef DEBUG
    printf("Allocated %d large objects\n", large_alloc_count);
    #endif // DEBUG
    
    // Check integrity
    check_pointers_integrity(objects, sizes, allocated);
    
    #ifdef DEBUG
    print_fancy(arena, 100);
    #endif // DEBUG

    TEST_PHASE("Random deallocation");
    // Randomly free objects
    freed_count = 0;
    int to_free = allocated / 2;
    
    for (int i = 0; i < to_free; i++) {
        int index = (i * 17 + 11) % allocated;
        if (objects[index]) {
            arena_free_block(objects[index]);
            objects[index] = NULL;
            sizes[index] = 0;
            freed_count++;
        }
    }
    
    ASSERT(freed_count > 0, "Should successfully free some objects randomly");
    
    #ifdef DEBUG
    printf("Freed %d objects randomly\n", freed_count);
    print_fancy(arena, 100);
    #endif // DEBUG
    
    TEST_PHASE("Fragmentation stress test");
    // Free even-indexed objects to create fragmentation
    freed_count = 0;
    
    for (int i = 0; i < allocated; i += 2) {
        if (objects[i]) {
            arena_free_block(objects[i]);
            objects[i] = NULL;
            sizes[i] = 0;
            freed_count++;
        }
    }
    
    ASSERT(freed_count > 0, "Should successfully free objects during fragmentation test");
    
    #ifdef DEBUG
    printf("Freed %d objects to fragment memory\n", freed_count);
    print_fancy(arena, 100);
    #endif // DEBUG
    
    TEST_PHASE("Allocation in fragmented arena");
    // Try to allocate in fragmented memory
    int frag_alloc_count = 0;
    pattern_errors = 0;
    
    for (int i = 0; i < 30; i++) {
        int size_pattern = i % 5;
        size_t size;
        
        switch (size_pattern) {
            case 0: size = 20;  break;
            case 1: size = 60;  break;
            case 2: size = 120; break;
            case 3: size = 30;  break;
            case 4: size = 90;  break;
        }
        
        void *ptr = arena_alloc(arena, size);
        if (ptr) {
            // Find an empty slot
            for (int j = 0; j < MAX_OBJECTS; j++) {
                if (objects[j] == NULL) {
                    objects[j] = ptr;
                    sizes[j] = size;
                    
                    // Fill with pattern
                    fill_memory_pattern(ptr, size, 300 + i);
                    
                    if (!verify_memory_pattern(ptr, size, 300 + i)) {
                        pattern_errors++;
                    }
                    
                    frag_alloc_count++;
                    break;
                }
            }
        }
    }
    
    ASSERT(frag_alloc_count > 0, "Should successfully allocate some objects in fragmented memory");
    ASSERT(pattern_errors == 0, "All objects in fragmented memory should have valid patterns");
    
    #ifdef DEBUG
    printf("Allocated %d objects in fragmented memory\n", frag_alloc_count);    
    print_fancy(arena, 100);
    #endif // DEBUG
    
    TEST_PHASE("Test arena reset");
    // Reset the arena and verify it's usable
    arena_reset(arena);
    ASSERT(arena->free_size_in_tail > 0, "Arena should have free space after reset");
    
    #ifdef DEBUG
    print_fancy(arena, 100);
    #endif // DEBUG

    // Try to allocate after reset
    void *post_reset_ptr = arena_alloc(arena, 100);
    ASSERT(post_reset_ptr != NULL, "Should be able to allocate memory after arena reset");
    arena_free_block(post_reset_ptr);

    #ifdef DEBUG
    printf("\n=== Final arena state ===\n");
    print_arena(arena);
    #endif // DEBUG
    
    arena_free(arena);
}

int main(void) {
    test_complex_allocation_pattern();
    
    // Print test summary
    print_test_summary();
    
    // Return non-zero exit code if any tests failed
    return tests_failed > 0 ? 1 : 0;
}