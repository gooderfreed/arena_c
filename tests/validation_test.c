#define ARENA_IMPLEMENTATION
#include "arena.h"
#include "test_utils.h"
#include <limits.h>
#include <stdint.h>

#include <limits.h>

#ifndef SSIZE_MAX
#define SSIZE_MAX (SIZE_MAX / 2)
#endif

void test_invalid_allocations(void) {
    TEST_PHASE("Invalid Allocation Scenarios");

    // Create an arena
    Arena *arena = arena_new_dynamic(1024);
    ASSERT(arena != NULL, "Arena creation should succeed");

    TEST_CASE("Zero size allocation");
    void *zero_size = arena_alloc(arena, 0);
    ASSERT(zero_size == NULL, "Zero size allocation should return NULL");

    TEST_CASE("Negative size allocation");
    void *negative_size = arena_alloc(arena, -1);
    ASSERT(negative_size == NULL, "Negative size allocation should return NULL");

    TEST_CASE("NULL arena allocation");
    void *null_arena = arena_alloc(NULL, 32);
    ASSERT(null_arena == NULL, "Allocation with NULL arena should return NULL");

    TEST_CASE("Free NULL pointer");
    arena_free_block(NULL); // Should not crash
    ASSERT(true, "Free NULL pointer should not crash");

    TEST_CASE("Free invalid pointer");
    Block fake_block = {0};
    fake_block.as.occupied.magic = (uintptr_t)(&fake_block + 1) ^ (uintptr_t)1;  // 0xFF is an invalid magic number
    void *fake_data = (char*)&fake_block + sizeof(Block);
    arena_free_block(fake_data); // Should not crash
    ASSERT(true, "Free invalid pointer should not crash");
    
    TEST_CASE("Free pointer from different arena");
    Arena *another_arena = arena_new_dynamic(1024);
    void *ptr = arena_alloc(another_arena, 32);
    arena_free_block(ptr); // Should not crash
    arena_free(another_arena);

    TEST_CASE("Free already freed pointer");
    void *ptr2 = arena_alloc(arena, 32);
    arena_free_block(ptr2);
    arena_free_block(ptr2); // Should not crash
    ASSERT(true, "Free already freed pointer should not crash");

    TEST_CASE("Allocation larger than arena size");
    void *huge_allocation = arena_alloc(arena, 2048);
    ASSERT(huge_allocation == NULL, "Allocation larger than arena size should fail");

    arena_free(arena);
}

void test_invalid_arena_creation(void) {
    TEST_PHASE("Invalid Arena Creation Scenarios");

    TEST_CASE("Zero size arena");
    Arena *zero_size_arena = arena_new_dynamic(0);
    ASSERT(zero_size_arena == NULL, "Zero size arena creation should fail");

    TEST_CASE("Negative size arena");
    Arena *negative_size_arena = arena_new_dynamic(-1);
    ASSERT(negative_size_arena == NULL, "Negative size arena creation should fail");

    TEST_CASE("Very large size arena");
    #if SIZE_MAX > 0xFFFFFFFF
        Arena *large_size_arena = arena_new_dynamic(SSIZE_MAX);
        ASSERT(large_size_arena == NULL, "Very large size arena creation should fail on 64-bit systems");
    #else
        printf("[INFO] Skipping SSIZE_MAX allocation test on 32-bit system.\n");
    #endif
    
    TEST_CASE("NULL memory for static arena");
    Arena *null_memory_arena = arena_new_static(NULL, 1024);
    ASSERT(null_memory_arena == NULL, "Static arena with NULL memory should fail");

    TEST_CASE("Negative size for static arena");
    void *mem = malloc(1024);
    Arena *negative_static_arena = arena_new_static(mem, -1);
    ASSERT(negative_static_arena == NULL, "Static arena with negative size should fail");
    free(mem);

    TEST_CASE("Free NULL arena");
    arena_free(NULL); // Should not crash
    ASSERT(true, "Free NULL arena should not crash");

    TEST_CASE("Reset NULL arena");
    arena_reset(NULL); // Should not crash
    ASSERT(true, "Reset NULL arena should not crash");
}

void test_boundary_conditions(void) {
    TEST_PHASE("Boundary Conditions");

    TEST_CASE("Arena size just above minimum");
    size_t min_size = ARENA_MIN_SIZE;
    Arena *min_size_arena = arena_new_dynamic(min_size);
    ASSERT(min_size_arena != NULL, "Arena with minimum valid size should succeed");
    arena_free(min_size_arena);

    TEST_CASE("Arena size just below minimum");
    Arena *below_min_arena = arena_new_dynamic(min_size - 1 - sizeof(Arena));
    ASSERT(below_min_arena == NULL, "Arena with size below minimum should fail");

    TEST_CASE("Static arena with minimum size");
    void *min_memory = malloc(min_size);
    Arena *min_static_arena = arena_new_static(min_memory, min_size);
    ASSERT(min_static_arena != NULL, "Static arena with minimum valid size should succeed");
    free(min_memory);

    TEST_CASE("Static arena with size below minimum");
    void *small_memory_bc = malloc(min_size - 1);
    Arena *small_static_arena_bc = arena_new_static(small_memory_bc, min_size - 1);
    ASSERT(small_static_arena_bc == NULL, "Static arena with size below minimum should fail");
    free(small_memory_bc);

    TEST_CASE("Tail allocation leaving fragment smaller than block header");
    size_t arena_size_frag = 1024;
    Arena *arena_frag = arena_new_dynamic(arena_size_frag);
    ASSERT(arena_frag != NULL, "Arena creation for fragmentation test should succeed");

    // Calculate initial free size in tail
    size_t initial_tail_free = free_size_in_tail(arena_frag);
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
    ASSERT(free_size_in_tail(arena_frag) == 0, "Tail free size should be 0 after small fragment alloc");

    arena_free(arena_frag);
}

void test_full_arena_allocation(void) {
    TEST_PHASE("Allocation in Full Arena");

    // Create an arena with minimal valid size
    // Size = Arena metadata + one Block metadata + minimal usable buffer
    size_t min_valid_size = BLOCK_MIN_SIZE + ARENA_DEFAULT_ALIGNMENT;
    Arena *arena = arena_new_dynamic(min_valid_size);
    ASSERT(arena != NULL, "Arena creation with minimal size should succeed");
    #ifdef DEBUG
    print_fancy(arena, 100);
    print_arena(arena);
    #endif // DEBUG

    TEST_CASE("Allocate block filling the entire initial tail");
    size_t avail = free_size_in_tail(arena);
    // Try to allocate exactly the minimum buffer size available
    void *first_block = arena_alloc(arena, avail);
    ASSERT(first_block != NULL, "Allocation of the first block should succeed");
    #ifdef DEBUG
    print_fancy(arena, 100);
    print_arena(arena);
    #endif // DEBUG

    // At this point, the free list should be empty, and the tail should have 0 free space.
    ASSERT(arena_get_free_blocks(arena) == NULL, "Free block list should be empty after filling allocation");
    ASSERT(free_size_in_tail(arena) == 0, "Free size in tail should be 0 after filling allocation");

    TEST_CASE("Attempt allocation when no space is left");
    void *second_block = arena_alloc(arena, 1); // Attempt to allocate just one more byte
    ASSERT(second_block == NULL, "Allocation should fail when no space is left");

    arena_free(arena);
}

void test_static_arena_creation(void) {
    TEST_PHASE("Static Arena Creation");

    TEST_CASE("Valid static arena creation");
    size_t static_arena_size = 2048;
    void *static_memory = malloc(static_arena_size);
    Arena *static_arena = arena_new_static(static_memory, static_arena_size);
    ASSERT(static_arena != NULL, "Static arena creation with valid memory should succeed");

    TEST_CASE("Allocation from static arena");
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
    TEST_PHASE("Freeing Invalid Blocks");

    // Create an arena
    Arena *arena = arena_new_dynamic(1024);
    ASSERT(arena != NULL, "Arena creation should succeed");

    TEST_CASE("Freeing a pointer not allocated by the arena");
    struct {
        uintptr_t fake_backlink;
        int data;
    } stack_obj;

    stack_obj.fake_backlink = (uintptr_t)&stack_obj.data ^ 1;
    stack_obj.data = 42;

    arena_free_block(&stack_obj.data); // Should safe return
    ASSERT(true, "Freeing stack variable should not crash");

    TEST_CASE("Freeing a pointer with valid magic number");
    Block fake_block = {0};
    fake_block.as.occupied.magic = 0xDEAFBEEF; // Valid magic number
    void *fake_data = (char*)&fake_block + sizeof(Block);
    arena_free_block(fake_data); // Should not crash
    ASSERT(true, "Freeing block with valid magic number should not crash");

    TEST_CASE("Freeing a pointer from a different arena");
    Arena *another_arena = arena_new_dynamic(1024);
    void *ptr = arena_alloc(another_arena, 32);
    arena_free_block(ptr); // Should not crash
    ASSERT(true, "Freeing block from different arena should not crash");
    arena_free(another_arena);

    arena_free(arena);
}

void test_calloc() {
    TEST_PHASE("Arena Calloc Functionality");

    // Create an arena
    Arena *arena = arena_new_dynamic(1024);
    ASSERT(arena != NULL, "Arena creation should succeed");
    
    #ifdef DEBUG
    print_fancy(arena, 100);
    print_arena(arena);
    #endif

    TEST_CASE("Calloc a block and verify zero-initialization");
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

    TEST_PHASE("Calloc with overflow in size calculation");
    ssize_t large_nmemb = SIZE_MAX / 2;
    ssize_t large_size = 3;

    void *p_overflow = arena_calloc(arena, large_nmemb, large_size);
    ASSERT(p_overflow == NULL, "Calloc with true overflow should return NULL");

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

void test_arena_reset_zero(void) {
    TEST_PHASE("Arena Reset Zero");

    TEST_CASE("Setup and dirtying memory");
    size_t arena_size = 4096;
    Arena *arena = arena_new_dynamic(arena_size);
    size_t arena_init_free_size = free_size_in_tail(arena);
    ASSERT(arena != NULL, "Dynamic arena creation should succeed");

    size_t data_size = 256;
    unsigned char *ptr1 = (unsigned char *)arena_alloc(arena, data_size);
    ASSERT(ptr1 != NULL, "Allocation 1 should succeed");
    
    memset(ptr1, 0xAA, data_size);
    ASSERT(ptr1[0] == 0xAA && ptr1[data_size - 1] == 0xAA, "Memory should be writable");

    unsigned char *ptr2 = (unsigned char *)arena_alloc(arena, data_size);
    ASSERT(ptr2 != NULL, "Allocation 2 should succeed");
    memset(ptr2, 0xBB, data_size);

    TEST_CASE("Execute reset_zero");
    arena_reset_zero(arena);
    ASSERT(free_size_in_tail(arena) > 0, "Arena should have free space after reset_zero");
    ASSERT(free_size_in_tail(arena) == arena_init_free_size, "Arena free size should be reset to initial state");

    TEST_CASE("Verify memory zeroing");

    int is_zero_1 = 1;
    for (size_t i = 0; i < data_size; i++) {
        if (ptr1[i] != 0) {
            is_zero_1 = 0;
            break;
        }
    }
    ASSERT(is_zero_1, "Memory at ptr1 should be strictly zeroed");

    int is_zero_2 = 1;
    for (size_t i = 0; i < data_size; i++) {
        if (ptr2[i] != 0) {
            is_zero_2 = 0;
            break;
        }
    }
    ASSERT(is_zero_2, "Memory at ptr2 (tail) should be strictly zeroed");

    TEST_CASE("Verify arena state reset");
    unsigned char *new_ptr = (unsigned char *)arena_alloc(arena, data_size);
    ASSERT(new_ptr != NULL, "Re-allocation after reset should succeed");
    ASSERT(new_ptr == ptr1, "Allocator should reset tail to the beginning");
    ASSERT(new_ptr[0] == 0, "New allocation should point to the zeroed memory");

    arena_free(arena);
}


// --- Alignment Abstraction Layer ---
#define TEST_BASE_ALIGNMENT 4096

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
    // C11 Standard
    #include <stdalign.h>
    #define ALIGN_PREFIX(N) alignas(N)
    #define ALIGN_SUFFIX(N)
    #define HAS_NATIVE_ALIGN 1

#elif defined(_MSC_VER)
    // MSVC
    #define ALIGN_PREFIX(N) __declspec(align(N))
    #define ALIGN_SUFFIX(N)
    #define HAS_NATIVE_ALIGN 1

#elif defined(__GNUC__) || defined(__clang__)
    // GCC / Clang (Extension)
    #define ALIGN_PREFIX(N)
    #define ALIGN_SUFFIX(N) __attribute__((aligned(N)))
    #define HAS_NATIVE_ALIGN 1

#else
    // C99 Fallback (No native support)
    #define ALIGN_PREFIX(N)
    #define ALIGN_SUFFIX(N)
    #define HAS_NATIVE_ALIGN 0
#endif


#if HAS_NATIVE_ALIGN
    #define BUFFER_OVERHEAD 0
#else
    #define BUFFER_OVERHEAD TEST_BASE_ALIGNMENT
#endif


static ALIGN_PREFIX(TEST_BASE_ALIGNMENT) 
char master_test_buffer[16384 + BUFFER_OVERHEAD] 
ALIGN_SUFFIX(TEST_BASE_ALIGNMENT);


static void* get_exact_alignment_ptr(size_t offset) {
    uintptr_t raw = (uintptr_t)master_test_buffer;
    uintptr_t base = (raw + (TEST_BASE_ALIGNMENT - 1)) & ~((uintptr_t)TEST_BASE_ALIGNMENT - 1);

    return (void*)(base + offset);
}


static size_t get_buffer_size(void *start) {
    uintptr_t raw = (uintptr_t)master_test_buffer;
    uintptr_t end = raw + 16384;
    uintptr_t aligned_start = (uintptr_t)start;

    return (end - aligned_start);
}


static size_t count_blocks_in_arena(Arena *arena) {
    size_t count = 0;
    Block *current = arena_get_first_block(arena);
    while (current != NULL) {
        count++;
        current = next_block(arena, current);
    }
    return count;
}


void test_alignment_alloc(void) {
    void *buffer = get_exact_alignment_ptr(8);
    size_t size = get_buffer_size(buffer);

    ASSERT(((uintptr_t)(buffer) % 8 == 0),   "Allocation should     be   8-byte aligned");
    ASSERT(((uintptr_t)(buffer) % 16 != 0),  "Allocation should not be  16-byte aligned");
    ASSERT(((uintptr_t)(buffer) % 32 != 0),  "Allocation should not be  32-byte aligned");
    ASSERT(((uintptr_t)(buffer) % 64 != 0),  "Allocation should not be  64-byte aligned");
    ASSERT(((uintptr_t)(buffer) % 128 != 0), "Allocation should not be 128-byte aligned");
    ASSERT(((uintptr_t)(buffer) % 256 != 0), "Allocation should not be 256-byte aligned");
    ASSERT(((uintptr_t)(buffer) % 512 != 0), "Allocation should not be 512-byte aligned");
    
    TEST_PHASE("Test alignment requirements with base 8-byte aligned arena");

    // ---------------------------------------------------------
    TEST_CASE("CASE 1: ReqAlign = 8 (Ideal)");
    {
        Arena *arena = arena_new_static_custom(buffer, size, 8);
        
        void *p1 = alloc_in_tail_full(arena, 50, 8);
        ASSERT(p1 != NULL, "Alloc should succeed");
        ASSERT((uintptr_t)p1 % 8 == 0, "Allocation should be properly 8-byte aligned");

        Block *tail = arena_get_first_block(arena);
        uintptr_t expected_data = (uintptr_t)tail + sizeof(Block);
        
        ASSERT((uintptr_t)p1 == expected_data, "Should correspond to zero padding");
        ASSERT(count_blocks_in_arena(arena) == 2, "No split should happen, only one block allocated in arena");
    }

    // ---------------------------------------------------------
    TEST_CASE("CASE 2: ReqAlign = 16 (Small Shift / XOR Link)");
    {
        Arena *arena = arena_new_static_custom(buffer, size, 8);
        Block *initial_first_block = arena_get_tail(arena);
        
        void *p2 = alloc_in_tail_full(arena, 50, 16);
        ASSERT(p2 != NULL, "Alloc should succeed");
        ASSERT((uintptr_t)p2 % 16 == 0, "Allocation should be properly 16-byte aligned");
        
        uintptr_t raw_data = (uintptr_t)initial_first_block + sizeof(Block);
        size_t padding = (uintptr_t)p2 - raw_data;
        ASSERT(padding == 8, "Padding should be exactly 8 bytes");
   
        ASSERT(arena_get_first_block(arena) == initial_first_block, "First block should not change (no split)");
        ASSERT(count_blocks_in_arena(arena) == 2, "No split should happen, only one block allocated in arena");
    }

    // ---------------------------------------------------------
    TEST_CASE("CASE 3: ReqAlign = 128 (Big Shift / Split)");
    {
        Arena *arena = arena_new_static_custom(buffer, size, 8);
        
        void *p3 = alloc_in_tail_full(arena, 50, 128);
        
        ASSERT(p3 != NULL, "Alloc should succeed");
        ASSERT((uintptr_t)p3 % 128 == 0, "Allocation should be properly 128-byte aligned");

        Block *new_first_block = arena_get_first_block(arena);
        ASSERT(new_first_block != p3 - sizeof(Block), "First block pointer MUST change (split happened)");
        ASSERT(count_blocks_in_arena(arena) == 3, "Split should happen, two blocks allocated in arena");
    }

    
    TEST_PHASE("Test Tail Absorption (Fill remaining space)");

    // ---------------------------------------------------------
    TEST_CASE("CASE 4: ReqAlign = 8 (Ideal + Absorb Tail)");
    {
        Arena *arena = arena_new_static_custom(buffer, size, 8);
        size_t capacity = free_size_in_tail(arena);
        void *p4 = alloc_in_tail_full(arena, capacity, 8);
        
        ASSERT(p4 != NULL, "Alloc should succeed");
        ASSERT((uintptr_t)p4 % 8 == 0, "Allocation should be properly 8-byte aligned");

        ASSERT(count_blocks_in_arena(arena) == 1, "Should absorb tail, leaving 1 block total");
        ASSERT(free_size_in_tail(arena) == 0, "Free space should be 0");
    }

    // ---------------------------------------------------------
    TEST_CASE("CASE 5: ReqAlign = 16 (Small Shift + Absorb Tail)");
    {
        Arena *arena = arena_new_static_custom(buffer, size, 8);
        size_t total_free = free_size_in_tail(arena);
        
        size_t padding = 8;
        size_t alloc_size = total_free - padding;

        void *p5 = alloc_in_tail_full(arena, alloc_size, 16);
        
        ASSERT(p5 != NULL, "Alloc should succeed");
        ASSERT((uintptr_t)p5 % 16 == 0, "Alignment check");
        
        ASSERT(count_blocks_in_arena(arena) == 1, "Should absorb tail with internal padding, 1 block total");
        ASSERT(free_size_in_tail(arena) == 0, "Free space should be 0");
    }

    // ---------------------------------------------------------
    TEST_CASE("CASE 6: ReqAlign = 128 (Big Shift/Split + Absorb Tail)");
    {
        Arena *arena = arena_new_static_custom(buffer, size, 8);
        size_t total_free = free_size_in_tail(arena);
        
        size_t padding = 103;
        
        size_t alloc_size = total_free - padding;

        void *p6 = alloc_in_tail_full(arena, alloc_size, 128);
        
        ASSERT(p6 != NULL, "Alloc should succeed");
        ASSERT((uintptr_t)p6 % 128 == 0, "Alignment check");

        ASSERT(count_blocks_in_arena(arena) == 2, "Split happened + Tail absorbed = 2 blocks total");
        ASSERT(free_size_in_tail(arena) == 0, "Free space should be 0");
        
        #ifdef DEBUG
        print_arena(arena);
        #endif
    }
}


int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0); 

    test_invalid_allocations();
    test_invalid_arena_creation();
    test_boundary_conditions();
    test_full_arena_allocation();
    test_static_arena_creation();
    test_freeing_invalid_blocks();
    test_calloc();
    test_arena_reset_zero();
    test_alignment_alloc();
    
    print_test_summary();
    return tests_failed > 0 ? 1 : 0;
} 