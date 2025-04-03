#define ARENA_IMPLEMENTATION
#include "arena.h"
#include "test_utils.h"

#define ARENA_SIZE (1024)
#define BLOCK_SIZE (32)
#define INITIAL_BLOCKS (10)
#define ADDITIONAL_BLOCKS (5)

void test_same_size_allocation(void) {
    TEST_CASE("Same Size Blocks Allocation Pattern");

    // Create an arena
    Arena *arena = arena_new_dynamic(ARENA_SIZE);
    ASSERT(arena != NULL, "Arena creation should succeed");

    void *blocks[INITIAL_BLOCKS + ADDITIONAL_BLOCKS] = {0};
    
    TEST_PHASE("Initial allocations");
    // Allocate INITIAL_BLOCKS blocks of the same size
    int allocated = 0;
    for (int i = 0; i < INITIAL_BLOCKS; i++) {
        blocks[allocated] = arena_alloc(arena, BLOCK_SIZE);
        ASSERT(blocks[allocated] != NULL, "Block allocation should succeed");
        
        // Fill with pattern to verify memory
        fill_memory_pattern(blocks[allocated], BLOCK_SIZE, i);
        ASSERT(verify_memory_pattern(blocks[allocated], BLOCK_SIZE, i), 
               "Memory pattern should be valid");
        
        allocated++;
    }

    #ifdef DEBUG
    printf("Initial state after %d allocations:\n", INITIAL_BLOCKS);
    print_fancy(arena, 100);
    print_arena(arena);
    #endif

    size_t after_initial_tail = arena->free_size_in_tail;
    
    TEST_PHASE("Free every second block");
    // Free every second block
    for (int i = 0; i < INITIAL_BLOCKS; i += 2) {
        arena_free_block(blocks[i]);
        blocks[i] = NULL;
    }

    #ifdef DEBUG
    printf("\nState after freeing every second block:\n");
    print_fancy(arena, 100);
    print_arena(arena);
    #endif

    TEST_PHASE("Additional allocations");
    // Try to allocate ADDITIONAL_BLOCKS more blocks
    int additional_allocated = 0;
    for (int i = 0; i < ADDITIONAL_BLOCKS; i++) {
        void *ptr = arena_alloc(arena, BLOCK_SIZE);
        ASSERT(ptr != NULL, "Additional block allocation should succeed");
        
        #ifdef DEBUG
        print_fancy(arena, 100);
        #endif
        // Find empty slot
        for (int j = 0; j < INITIAL_BLOCKS + ADDITIONAL_BLOCKS; j++) {
            if (blocks[j] == NULL) {
                blocks[j] = ptr;
                // Fill with pattern
                fill_memory_pattern(ptr, BLOCK_SIZE, 100 + i);
                ASSERT(verify_memory_pattern(ptr, BLOCK_SIZE, 100 + i),
                       "Additional block memory pattern should be valid");
                additional_allocated++;
                break;
            }
        }
    }

    ASSERT(additional_allocated == ADDITIONAL_BLOCKS, 
           "Should allocate all additional blocks");

    #ifdef DEBUG
    printf("\nFinal state after additional allocations:\n");
    print_fancy(arena, 100);
    print_arena(arena);
    #endif

    // Verify final state
    ASSERT(arena->free_size_in_tail == after_initial_tail,
           "Tail size should be the same as after initial allocations");
    ASSERT(arena->free_blocks == NULL, "Free block should be NULL");

    
    // Free all remaining blocks
    for (int i = 0; i < INITIAL_BLOCKS + ADDITIONAL_BLOCKS; i++) {
        if (blocks[i] != NULL) {
            arena_free_block(blocks[i]);
            #ifdef DEBUG
            print_fancy(arena, 100);
            #endif
        }
    }

    arena_free(arena);
}

int main(void) {
    test_same_size_allocation();
    
    print_test_summary();
    return tests_failed > 0 ? 1 : 0;
} 