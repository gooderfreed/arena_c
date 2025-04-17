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
            default:
                // This should never happen due to modulo 5,
                // but better to have a sane default
                size = 20;
                break;
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

void test_block_merging(void) {
    TEST_CASE("Block Merging and Fragmentation");

    // Create an arena
    Arena *arena = arena_new_dynamic(ARENA_SIZE/10);
    ASSERT(arena != NULL, "Arena creation should succeed");

    // Allocate three blocks of 1KB each
    size_t block_size = 100;
    void *block1 = arena_alloc(arena, block_size);
    void *block2 = arena_alloc(arena, block_size);
    void *block3 = arena_alloc(arena, block_size);
    
    ASSERT(block1 != NULL && block2 != NULL && block3 != NULL, 
           "Should successfully allocate three blocks");

    #ifdef DEBUG
    printf("\nInitial state after three allocations:\n");
    print_fancy(arena, 100);
    #endif // DEBUG

    // Free first two blocks
    arena_free_block(block1);
    arena_free_block(block2);

    #ifdef DEBUG
    printf("\nState after freeing first two blocks:\n");
    print_fancy(arena, 100);
    #endif // DEBUG

    // Try to allocate a block that fits exactly in the space of two freed blocks
    // Size = 2 * block_size + sizeof(Block) (for metadata)
    size_t merged_size = 2 * block_size + sizeof(Block);
    void *merged_block = arena_alloc(arena, merged_size);
    ASSERT(merged_block != NULL, "Should successfully allocate merged block");
    
    #ifdef DEBUG
    printf("\nState after allocating merged block:\n");
    print_fancy(arena, 100);
    #endif // DEBUG

    // Free the merged block
    arena_free_block(merged_block);

    // Try to allocate a block that's slightly smaller than the merged space
    // This should create a new free block from the remaining space
    size_t smaller_size = merged_size - sizeof(Block) - MIN_BUFFER_SIZE;
    void *smaller_block = arena_alloc(arena, smaller_size);
    ASSERT(smaller_block != NULL, "Should successfully allocate smaller block");

    #ifdef DEBUG
    printf("\nState after allocating smaller block:\n");
    print_fancy(arena, 100);
    #endif // DEBUG

    // Verify that a new free block was created
    ASSERT(arena->free_blocks != NULL, "Should have a free block from remaining space");
    ASSERT(arena->free_blocks->size == MIN_BUFFER_SIZE, "Free block should have exactly MIN_BUFFER_SIZE");

    // Free the smaller block
    arena_free_block(smaller_block);

    // Try to allocate a block that's just 1 byte too large to cause fragmentation
    size_t no_split_size = merged_size - sizeof(Block) - MIN_BUFFER_SIZE + 1;
    void *no_split_block = arena_alloc(arena, no_split_size);
    ASSERT(no_split_block != NULL, "Should successfully allocate block without splitting");
    
    #ifdef DEBUG
    printf("\nState after allocating no split block:\n");
    print_fancy(arena, 100);
    #endif // DEBUG

    ASSERT(arena->free_blocks == NULL, "Should not have any free blocks after allocation");

    arena_free(arena);
}

// Test specific LLRB detach scenarios
void test_llrb_detach_scenarios(void) {
    TEST_CASE("LLRB Detach Scenarios");

    // Scenario: Detach Root (and empty the tree)
    TEST_PHASE("Detach Root Node");
    Arena *arena_root = arena_new_dynamic(1024);
    ASSERT(arena_root != NULL, "[Detach Root] Arena creation should succeed");
    #ifdef DEBUG
    print_arena(arena_root);
    print_fancy(arena_root, 100);
    #endif // DEBUG

    // Allocate two blocks, so the first one is not the tail
    void *ptr_a_root = arena_alloc(arena_root, 100);
    #ifdef DEBUG
    print_fancy(arena_root, 100);
    #endif // DEBUG
    void *ptr_b_root = arena_alloc(arena_root, 200);
    #ifdef DEBUG
    print_fancy(arena_root, 100);
    #endif // DEBUG
    ASSERT(ptr_a_root != NULL && ptr_b_root != NULL, "[Detach Root] Initial allocations should succeed");

    // Free the first block (A). It should become the root of the free tree.
    arena_free_block(ptr_a_root);
    #ifdef DEBUG
    print_fancy(arena_root, 100);
    #endif // DEBUG
    ASSERT(arena_root->free_blocks != NULL, "[Detach Root] Free list should contain block A");
    ASSERT(arena_root->free_blocks->size == 100, "[Detach Root] Root of free list should be block A");

    // Allocate the same size again (100). This should find block A and detach it.
    void *ptr_c_root = arena_alloc(arena_root, 100);
    #ifdef DEBUG
    print_fancy(arena_root, 100);
    #endif // DEBUG
    ASSERT(ptr_c_root != NULL, "[Detach Root] Allocation reusing block A should succeed");
    ASSERT(ptr_c_root == ptr_a_root, "[Detach Root] Reused block should be the same memory as A");

    // The free tree should now be empty
    ASSERT(arena_root->free_blocks == NULL, "[Detach Root] Free list should be empty after detaching root");

    arena_free(arena_root);



    // Scenario: Detach Right Child
    TEST_PHASE("Detach Right Child Node");
    Arena *arena_right = arena_new_dynamic(2048);
    ASSERT(arena_right != NULL, "[Detach Right] Arena creation should succeed");
    #ifdef DEBUG
    print_arena(arena_right);
    print_fancy(arena_right, 100);
    #endif // DEBUG

    // Allocate blocks, free them in a specific order to (hopefully) create B -> C structure
    void *ptr_a_right = arena_alloc(arena_right, 50);
    void *ptr_b_right = arena_alloc(arena_right, 150);
    void *ptr_c_right = arena_alloc(arena_right, 200);
    ASSERT(ptr_a_right && ptr_b_right && ptr_c_right, "[Detach Right] Initial allocations should succeed");
    #ifdef DEBUG
    print_fancy(arena_right, 100);
    #endif // DEBUG

    arena_free_block(ptr_b_right); // Free 150 first (potential root)
    #ifdef DEBUG
    print_fancy(arena_right, 100);
    #endif // DEBUG

    arena_free_block(ptr_a_right); // Free 50 (potential left child)
    #ifdef DEBUG
    print_fancy(arena_right, 100);
    #endif // DEBUG

    arena_free_block(ptr_c_right); // Free 200 (potential right child)
    #ifdef DEBUG
    print_fancy(arena_right, 100);
    #endif // DEBUG
    // Exact tree structure depends on LLRB insert/balance, but C is likely inserted to the right of B

    // Allocate the right child's size
    void *ptr_d_right = arena_alloc(arena_right, 200);
    ASSERT(ptr_d_right != NULL, "[Detach Right] Allocation reusing the right child block should succeed");
    #ifdef DEBUG
    print_arena(arena_right);
    print_fancy(arena_right, 100);
    #endif // DEBUG
    // We can't easily assert the tree structure changed correctly without exposing internals,
    // but if this allocation succeeds and doesn't crash, it implies detach handled the right child case.

    arena_free(arena_right);
}

int main(void) {
    test_complex_allocation_pattern();
    test_block_merging();
    test_llrb_detach_scenarios();

    // Print test summary
    print_test_summary();
    
    // Return non-zero exit code if any tests failed
    return tests_failed > 0 ? 1 : 0;
}