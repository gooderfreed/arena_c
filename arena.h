#ifndef ARENA_ALLOCATOR_H
#define ARENA_ALLOCATOR_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>


#if defined(_MSVC_VER)
#include <intrin.h>
#endif


#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L) || defined(__cplusplus)
#   include <assert.h>
#   define ARENA_STATIC_ASSERT(cond, msg) static_assert(cond, #msg)
#else
#   define ARENA_STATIC_ASSERT_HELPER(cond, line) typedef char static_assertion_at_line_##line[(cond) ? 1 : -1]
#   define ARENA_STATIC_ASSERT(cond, msg) ARENA_STATIC_ASSERT_HELPER(cond, __LINE__)
#endif


    
#ifdef DEBUG
    #include <assert.h>
    #define ARENA_ASSERT(cond) assert(cond)
#else
    #if defined(__GNUC__) || defined(__clang__)
        #define ARENA_ASSERT(cond) do { if (!(cond)) __builtin_unreachable(); } while(0)
    #elif defined(_MSVC_VER)
        #define ARENA_ASSERT(cond) __assume(cond)
    #else
        #define ARENA_ASSERT(cond) ((void)0)
    #endif
#endif


#ifndef ARENA_MIN_BUFFER_SIZE
    // Default minimum buffer size for the arena.
#   define ARENA_MIN_BUFFER_SIZE 16 
#endif
ARENA_STATIC_ASSERT(ARENA_MIN_BUFFER_SIZE > 0, "MIN_BUFFER_SIZE must be a positive value to prevent creation of useless zero-sized free blocks.");


#define ARENA_DEFAULT_ALIGNMENT 16 // Default memory alignment


#if defined(__GNUC__) || defined(__clang__)
    #define MIN_EXPONENT (__builtin_ctz(sizeof(uintptr_t)))
#else
    #define MIN_EXPONENT ( \
        (sizeof(uintptr_t) == 4) ? 2 : \
        (sizeof(uintptr_t) == 8) ? 3 : \
        4                              \
    )
#endif

#define MAX_ALIGNMENT ((size_t)(256 << MIN_EXPONENT))
#define MIN_ALIGNMENT ((size_t)sizeof(uintptr_t))

#define ALIGNMENT_MASK  ((uintptr_t)7)
#define SIZE_MASK       (~(uintptr_t)7)
#define IS_FREE_FLAG    ((uintptr_t)1)
#define COLOR_FLAG      ((uintptr_t)2) 
#define PREV_MASK       (~(uintptr_t)3)
#define IS_DYNAMIC_FLAG ((uintptr_t)1)
#define IS_NESTED_FLAG  ((uintptr_t)2)
#define TAIL_MASK       ((uintptr_t)~3)

#define RED false
#define BLACK true

#define BLOCK_MIN_SIZE (sizeof(Block) + ARENA_MIN_BUFFER_SIZE)
#define ARENA_MIN_SIZE (sizeof(Arena) + BLOCK_MIN_SIZE)

#define block_data(block) ((void *)((char *)(block) + sizeof(Block)))

// Structure type declarations for memory management
typedef struct Block Block;
typedef struct Arena Arena;
typedef struct Bump  Bump;

/*
 * Memory block structure
 * Represents a chunk of memory and metadata for its management within the arena
 */
struct Block {
    size_t size_and_alignment;   // Size of the data block.
    Block *prev;                 // Pointer to the previous block in the global list, also stores flags via pointer tagging.

    union {
        struct {
            Block *left_free;     // Left child in red-black tree
            Block *right_free;    // Right child in red-black tree
        } free;
        struct {
            Arena *arena;         // Pointer to the arena that allocated this block
            uintptr_t magic;      // Magic number for validation random pointer
        } occupied;
    } as;
};

/*
 * Bump allocator structure
 * A simple allocator that allocates memory linearly from a pre-allocated block
 */
struct Bump {
    union {
        Block block_representation;  // Block representation for compatibility
        struct {
            size_t capacity;         // Total capacity of the bump allocator
            Block *prev;             // Pointer to the previous block in the global list, need for compatibility with block struct layout
            Arena *arena;            // Pointer to the arena that allocated this block
            size_t offset;           // Current offset for allocations within the bump allocator
        } self;
    } as;
};

ARENA_STATIC_ASSERT((sizeof(Bump) == sizeof(Block)), Size_mismatch_between_Bump_and_Block);

/*
 * Memory arena structure
 * Manages a pool of memory, block allocation, and block states
 */
struct Arena {
    union {
        Block block_representation;         // Block representation for compatibility
        struct {
            size_t capacity_and_alignment;  // Total capacity of the arena
            Block *prev;                    // Pointer to the previous block in the global list, need for compatibility with block struct layout
            Block *tail;                    // Pointer to the last block in the global list, also stores is_dynamic flag via pointer tagging
            Block *free_blocks;             // Pointer to the tree of free blocks
        } self;
    } as;
};

ARENA_STATIC_ASSERT((sizeof(Bump) == sizeof(Block)), Size_mismatch_between_Arena_and_Block);


#ifdef DEBUG
#include <stdio.h>
#include <math.h>
void print_arena(Arena *arena);
void print_fancy(Arena *arena, size_t bar_size);
void print_llrb_tree(Block *node, int depth);
#endif // DEBUG


#ifndef ARENA_NO_MALLOC
Arena *arena_new_dynamic(size_t size);
Arena *arena_new_dynamic_custom(size_t size, size_t alignment);
#endif // ARENA_NO_MALLOC

Arena *arena_new_static(void *memory, size_t size);
Arena *arena_new_static_custom(void *memory, size_t size, size_t alignment);
void arena_reset(Arena *arena);
void arena_free(Arena *arena);

void *arena_alloc(Arena *arena, size_t size);
void *arena_alloc_custom(Arena *arena, size_t size, size_t alignment);
void *arena_calloc(Arena *arena, size_t nmemb, size_t size);
void arena_reset_zero(Arena *arena);
void arena_free_block(void *data);

Bump *bump_new(Arena *parent_arena, size_t size);
void *bump_alloc(Bump *bump, size_t size);
void *bump_alloc_aligned(Bump *bump, size_t size, size_t alignment);
void bump_trim(Bump *bump);
void bump_reset(Bump *bump);
void bump_free(Bump *bump);



#ifdef ARENA_IMPLEMENTATION

/*
 * Helper function to Align up
 * Rounds up the given size to the nearest multiple of alignment
 */
static inline size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

/*
 * Helper function to find minimum exponent of a number
 * Returns the position of the least significant set bit
 */
static inline size_t min_exponent_of(size_t num) {
    if (num == 0) return 0; // Undefined for zero, return 0 as a safe default

    // Use compiler built-ins if available for efficiency
    #if defined(__GNUC__) || defined(__clang__)
        return __builtin_ctz(num);
    #elif defined(_MSVC_VER)
        unsigned long index;
        #if defined(_M_X64) || defined(_M_ARM64)
            _BitScanForward64(&index, num);
        #else
            _BitScanForward(&index, num);
        #endif
        return index;
    #else
        size_t s = num;
        size_t zeros = 0;
        while ((s >>= 1) != 0) ++zeros;
        return zeros;
    #endif
}

/*
 * Get alignment from block
 * Extracts the alignment information stored in the block's size_and_alignment field
 */
static inline size_t get_alignment(const Block *block) {
    ARENA_ASSERT((block != NULL) && "Internal Error: 'get_alignment' called on NULL block");

    size_t exponent = (block->size_and_alignment & ALIGNMENT_MASK) + MIN_EXPONENT; // Extract exponent and adjust by MIN_EXPONENT
    size_t alignment = (size_t)1 << (exponent); // Calculate alignment as power of two

    return alignment;
}

/*
 * Set alignment for block
 * Updates the alignment information in the block's size_and_alignment field
 * Valid alignment range (power of two):
 *  -- 32 bit system: [4 ... 512]
 *  -- 64 bit system: [8 ... 1024]
 */
static inline void set_alignment(Block *block, size_t alignment) {
    ARENA_ASSERT((block != NULL)                      && "Internal Error: 'set_alignment' called on NULL block");
    ARENA_ASSERT(((alignment & (alignment - 1)) == 0) && "Internal Error: 'set_alignment' called on invalid alignment");
    ARENA_ASSERT((alignment >= MIN_ALIGNMENT)         && "Internal Error: 'set_alignment' called on too small alignment");
    ARENA_ASSERT((alignment <= MAX_ALIGNMENT)         && "Internal Error: 'set_alignment' called on too big alignment");

    /*
     * How does that work?
     * Alignment is always a power of two, so instead of storing the alignment directly and wasting full 4-8 bytes, we can represent it as 2^n.
     * Since minimum alignment is 2^MIN_EXPONENT, we can store only the exponent minus MIN_EXPONENT in 3 bits(value 0-7).
     * For example:
     *  - On 32-bit system (MIN_EXPONENT = 2):
     *       Alignment 4     ->  2^2  ->  2-2  ->  stored as 0
     *       Alignment 8     ->  2^3  ->  3-2  ->  stored as 1
     *       Alignment 16    ->  2^4  ->  4-2  ->  stored as 2
     *       ... and so on up to
     *       Alignment 512   ->  2^9  ->  9-2  ->  stored as 7
     * 
     *  - On 64-bit system (MIN_EXPONENT = 3):
     *       Alignment 8     ->  2^3  ->  3-3  ->  stored as 0
     *       Alignment 16    ->  2^4  ->  4-3  ->  stored as 1
     *       Alignment 32    ->  2^5  ->  5-3  ->  stored as 2
     *       ... and so on up to 
     *       Alignment 1024  -> 2^10  -> 10-3  ->  stored as 7
     * This way, we efficiently use only 3 bits to cover the full range alignments that could be potentially used within the size_and_alignment field.
    */ 
    
    size_t exponent = min_exponent_of(alignment >> MIN_EXPONENT); // Calculate exponent from alignment

    size_t spot = block->size_and_alignment & ALIGNMENT_MASK; // Preserve current alignment bits
    block->size_and_alignment = block->size_and_alignment ^ spot; // Clear current alignment bits

    block->size_and_alignment = block->size_and_alignment | exponent;  // Set new alignment bits
}



/*
 * Get size from block
 * Extracts the size information stored in the block's size_and_alignment field
 */
static inline size_t get_size(const Block *block) {
    ARENA_ASSERT((block != NULL) && "Internal Error: 'get_size' called on NULL block");

    return block->size_and_alignment >> 3;
}

/*
 * Set size for block
 * Updates the size information in the block's size_and_alignment field
 * Valid size range (limited by 3-bit reserved for alignment/flags):
 *  -- 32-bit system: [0] U [ARENA_MIN_BUFFER_SIZE ... 512 MiB] (2^29 bytes)
 *  -- 64-bit system: [0] U [ARENA_MIN_BUFFER_SIZE ... 2 EiB]   (2^61 bytes)
 */
static inline void set_size(Block *block, size_t size) {
    ARENA_ASSERT((block != NULL)      && "Internal Error: 'set_size' called on NULL block");
    ARENA_ASSERT((size <= SIZE_MASK)  && "Internal Error: 'set_size' called on too big size");

    /*
     * Why size limit?
     * Since we utilize 3 bits of size_and_alignment field for alignment/flags, we have the remaining bits available for size.
     * 
     * On 32-bit systems, size_t is 4 bytes (32 bits), so we have 29 bits left for size (32 - 3 = 29).
     * This gives us a maximum size of 2^29 - 1 = 536,870,911 bytes (approximately 512 MiB).
     * In 32-bit systems, where maximum addressable memory in user space is 2-3 GiB, this limitation is acceptable.
     * Bigger size is not practical since we cannot allocate a contiguous memory block that **literally** 30%+ of all accessible memory, 
     *  malloc is extremely likely to return NULL due to heap fragmentation.
     * Like, what you even gonna do with 1GB of contiguous memory, when even all operating system use ~1-2GB?
     * Play "Bad Apple" 8K 120fps via raw frames?
     * 
     * On the other hand,
     * 
     * On 64-bit systems, size_t is 8 bytes (64 bits), so we have 61 bits left for size (64 - 3 = 61).
     * This gives us a maximum size of 2^61 - 1 = 2,305,843,009,213,693,951 bytes (approximately 2 EiB).
     * In 64-bit systems, this limitation is practically non-existent since current hardware and OS limitations are far below this threshold.
     * 
     * Conclusion: This limitation is a deliberate trade-off that avoids any *real* constraints on both 32-bit and 64-bit systems while optimizing memory usage.
    */

    size_t alignment_piece = block->size_and_alignment & ALIGNMENT_MASK; // Preserve current alignment bits
    block->size_and_alignment = (size << 3) | alignment_piece; // Set new size while preserving alignment bits
}



/*
 * Get pointer to prev block from given block
 * Extracts the previous block pointer stored in the block's prev field
 */
static inline Block *get_prev(const Block *block) {
    ARENA_ASSERT((block != NULL) && "Internal Error: 'get_prev' called on NULL block");

    return (Block *)((uintptr_t)block->prev & PREV_MASK); // Clear flag bits to get actual pointer
}

/*
 * Set pointer to prev block for given block
 * Updates the previous block pointer in the block's prev field
 */
static inline void set_prev(Block *block, void *ptr) {
    ARENA_ASSERT((block != NULL) && "Internal Error: 'set_prev' called on NULL block");

    /*
     * Why pointer tagging?
     * Classic approach would be to have separate fields for pointer and flags, 
     *  but that would increase the size of the Block struct by an additional 8-16 bytes if kept separate,
     *  or by 4-8 bytes if we use bitfields.
     * Any of this ways would bloat the Block struct size dramatically, especially when we have tons of blocks in the arena.
     * 
     * Instead, by knowing that pointers are always aligned to at least 4 bytes (on 32-bit systems) or 8 bytes (on 64-bit systems),
     *  we can utilize that free space in the 2-3 least significant bits of the pointer to store our flags.
     * But because we want to have 32-bit support as well, we can only safely use 2 bits for flags,
     *  since on 32-bit systems, pointers are aligned to at least 4 bytes, meaning the last 2 bits are always zero.
     * 
     * This way, we can store our flags without increasing the size of the Block struct at all.
    */
    
    uintptr_t flags_tips = (uintptr_t)block->prev & ~PREV_MASK; // Preserve flag bits
    block->prev = (Block *)((uintptr_t)ptr | flags_tips); // Set new pointer while preserving flag bits
}



/*
 * Get is_free flag from block
 * Extracts the is_free flag stored in the block's prev field
 */
static inline bool get_is_free(const Block *block) {
    ARENA_ASSERT((block != NULL) && "Internal Error: 'get_is_free' called on NULL block");

    return (uintptr_t)block->prev & IS_FREE_FLAG; // Check the is_free flag bit
}

/*
 * Set is_free flag for block
 * Updates the is_free flag in the block's prev field
 */
static inline void set_is_free(Block *block, bool is_free) {
    ARENA_ASSERT((block != NULL) && "Internal Error: 'set_is_free' called on NULL block");

    /*
     * See 'set_prev' for explanation of pointer tagging.
     * Here we use 1st least significant bit to store is_free flag. 
    */

    uintptr_t int_ptr = (uintptr_t)(block->prev); // Get current pointer with flags
    if (is_free) {
        int_ptr |= IS_FREE_FLAG;  // Set the is_free flag bit
    }
    else {
        int_ptr &= ~IS_FREE_FLAG; // Clear the is_free flag bit
    }
    block->prev = (Block *)int_ptr; // Update the prev field with new flags
}



/*
 * Get color flag from block
 * Extracts the color flag stored in the block's prev field
 */
static inline bool get_color(Block *block) {
    ARENA_ASSERT((block != NULL) && "Internal Error: 'get_color' called on NULL block");

    return ((uintptr_t)block->prev & COLOR_FLAG); // Check the color flag bit
}

/*
 * Set color flag for block
 * Updates the color flag in the block's prev field
 */
static inline void set_color(Block *block, bool color) {
    ARENA_ASSERT((block != NULL) && "Internal Error: 'set_color' called on NULL block");

    /*
     * See 'set_prev' for explanation of pointer tagging.
     * Here we use 2nd least significant bit to store color flag. 
    */

    uintptr_t int_ptr = (uintptr_t)(block->prev); // Get current pointer with flags
    if (color) {
        int_ptr |= COLOR_FLAG; // Set the color flag bit
    }
    else {
        int_ptr &= ~COLOR_FLAG; // Clear the color flag bit
    }
    block->prev = (Block *)int_ptr; // Update the prev field with new flags
}



/*
 * Get left child from block
 * Extracts the left child pointer stored in the block's as.free.left_free field
 */
static inline Block *get_left_tree(const Block *block) {
    ARENA_ASSERT((block != NULL) && "Internal Error: 'get_left_tree' called on NULL block");

    return block->as.free.left_free; // Return left child pointer
}

/*
 * Set left child for block
 * Updates the left child pointer in the block's as.free.left_free field
 */
static inline void set_left_tree(Block *parent_block, Block *left_child_block) {
    ARENA_ASSERT((parent_block != NULL) && "Internal Error: 'set_left_tree' called on NULL parent_block");

    parent_block->as.free.left_free = left_child_block; // Set left child pointer
}



/*
 * Get right child from block
 * Extracts the right child pointer stored in the block's as.free.right_free field
 */
static inline Block *get_right_tree(const Block *block) {
    ARENA_ASSERT((block != NULL) && "Internal Error: 'get_right_tree' called on NULL block");

    return block->as.free.right_free; // Return right child pointer
}

/*
 * Set right child for block
 * Updates the right child pointer in the block's as.free.right_free field
 */
static inline void set_right_tree(Block *parent_block, Block *right_child_block) {
    ARENA_ASSERT((parent_block != NULL) && "Internal Error: 'set_right_tree' called on NULL parent_block");

    parent_block->as.free.right_free = right_child_block; // Set right child pointer
}



/*
 * Get magic number from block
 * Extracts the magic number stored in the block's as.occupied.magic field
 */
static inline uintptr_t get_magic(const Block *block) {
    ARENA_ASSERT((block != NULL) && "Internal Error: 'get_magic' called on NULL block");

    return block->as.occupied.magic; // Return magic number
}

/*
 * Set magic number for block
 * Updates the magic number in the block's as.occupied.magic field
 */
static inline void set_magic(Block *block, void *user_ptr) {
    ARENA_ASSERT((block != NULL)    && "Internal Error: 'set_magic' called on NULL block");
    ARENA_ASSERT((user_ptr != NULL) && "Internal Error: 'set_magic' called on NULL user_ptr");

    /*
     * Why use magic and XOR with user pointer?
     * 
     * Arena_c main goal is to provide a simple external API, and 'free' is one of the most critical functions.
     * by allowing users to pass only the user pointer to 'free', we need a way to verify that the pointer is valid and was indeed allocated by our arena.
     * Using a magic number helps us achieve this by providing a unique identifier for each allocated block.
     * 
     * But storing a fixed magic number can be predictable and potentially exploitable.
     * By XORing the magic number with the user pointer, we create a unique magic value for each allocation.
     * This makes it significantly harder for an attacker to guess or forge valid magic numbers,
     *  enhancing the security and integrity of the memory management system.
    */

    block->as.occupied.magic = (uintptr_t)0xDEADBEEF ^ (uintptr_t)user_ptr; // Set magic number using XOR with user pointer
}

/*
 * Validate magic number for block
 * Checks if the magic number in the block matches the expected value based on the user pointer
 */
static inline bool is_valid_magic(const Block *block, const void *user_ptr) {
    ARENA_ASSERT((block != NULL)    && "Internal Error: 'is_valid_magic' called on NULL block");
    ARENA_ASSERT((user_ptr != NULL) && "Internal Error: 'is_valid_magic' called on NULL user_ptr");

    return ((get_magic(block) ^ (uintptr_t)user_ptr) == (uintptr_t)0xDEADBEEF); // Validate magic number by XORing with user pointer
}



/*
 * Get arena from block
 * Extracts the arena pointer stored in the block's as.occupied.arena field
 */
static inline Arena *get_arena(const Block *block) {
    ARENA_ASSERT((block != NULL) && "Internal Error: 'get_arena' called on NULL block");

    return block->as.occupied.arena; // Return arena pointer
}

/*
 * Set arena for block
 * Updates the arena pointer in the block's as.occupied.arena field
 */
static inline void set_arena(Block *block, Arena *arena) {
    ARENA_ASSERT((block != NULL) && "Internal Error: 'set_arena' called on NULL block");
    ARENA_ASSERT((arena != NULL) && "Internal Error: 'set_magic' called on NULL arena");

    block->as.occupied.arena = arena; // Set arena pointer
}





/*
 * Arena accessors and mutators
 * Functions to get and set various properties of the Arena structure
 */
static inline Block *arena_get_tail(const Arena *arena) {
    ARENA_ASSERT((arena != NULL) && "Internal Error: 'arena_get_tail' called on NULL arena");

    return (Block *)((uintptr_t)arena->as.self.tail & TAIL_MASK); // Clear the is_dynamic flag bit to get actual pointer
}

/*
 * Set tail block for arena
 * Updates the tail block pointer in the arena's as.self.tail field
 */
static inline void arena_set_tail(Arena *arena, Block *block) {
    ARENA_ASSERT((arena != NULL) && "Internal Error: 'arena_set_tail' called on NULL arena");
    ARENA_ASSERT((block != NULL) && "Internal Error: 'arena_set_tail' called on NULL block");

    /* 
     * See 'set_prev' for explanation of pointer tagging.
     * In this case we store is_dynamic flag in the tail pointer.
    */

    uintptr_t flags_tips = (uintptr_t)arena->as.self.tail & ~TAIL_MASK; // Preserve flag bits
    arena->as.self.tail = (Block *)((uintptr_t)block | flags_tips); // set new pointer while preserving flag bits
}



/*
 * Get is_dynamic flag from arena
 * Extracts the is_dynamic flag stored in the arena's as.self.tail field
 */
static inline bool arena_get_is_dynamic(const Arena *arena) {
    ARENA_ASSERT((arena != NULL) && "Internal Error: 'arena_get_is_dynamic' called on NULL arena");

    return ((uintptr_t)arena->as.self.tail & IS_DYNAMIC_FLAG); // Check the is_dynamic flag bit
}

/*
 * Set is_dynamic flag for arena
 * Updates the is_dynamic flag in the arena's as.self.tail field
 */
static inline void arena_set_is_dynamic(Arena *arena, bool is_dynamic) {
    ARENA_ASSERT((arena != NULL) && "Internal Error: 'arena_set_is_dynamic' called on NULL arena");

    /*
     * See 'set_prev' for explanation of pointer tagging.
     * Here we use 1st least significant bit to store is_dynamic flag. 
    */

    uintptr_t int_ptr = (uintptr_t)(arena->as.self.tail); // Get current pointer with flags
    if (is_dynamic) {
        int_ptr |= IS_DYNAMIC_FLAG; // Set the is_dynamic flag bit
    }
    else {
        int_ptr &= ~IS_DYNAMIC_FLAG; // Clear the is_dynamic flag bit
    }
    arena->as.self.tail = (Block *)int_ptr; // Update the tail field with new flags
}



/*
 * Get is arena nested
 * Retrieves the is_nested flag of the arena pointer
 */
static inline bool arena_get_is_nested(const Arena *arena) {
    ARENA_ASSERT((arena != NULL) && "Internal Error: 'arena_get_is_nested' called on NULL arena");

    return ((uintptr_t)arena->as.self.tail & IS_NESTED_FLAG);
}

/*
 * Set is arena nested
 * Updates the is_nested flag of the arena pointer
 */
static inline void arena_set_is_nested(Arena *arena, bool is_nested) {
    ARENA_ASSERT((arena != NULL) && "Internal Error: 'arena_set_is_nested' called on NULL arena");

    /*
     * See 'set_prev' for explanation of pointer tagging.
     * Here we use 2st least significant bit to store is_nested flag. 
    */

    uintptr_t int_ptr = (uintptr_t)(arena->as.self.tail);  // Get current pointer with flags
    if (is_nested) {
        int_ptr |= IS_NESTED_FLAG; // Set the is_nested flag bit
    }
    else {
        int_ptr &= ~IS_NESTED_FLAG; // Clear the is_nested flag bit
    }
    arena->as.self.tail = (Block *)int_ptr; // Update the tail field with new flags
}


/*
 * Get free blocks tree from arena
 * Extracts the pointer to the root of the free blocks tree stored in the arena's as.self.free_blocks field
 */
static inline Block *arena_get_free_blocks(const Arena *arena) {
    ARENA_ASSERT((arena != NULL) && "Internal Error: 'arena_get_free_blocks' called on NULL arena");

    return arena->as.self.free_blocks; // Return pointer to the root of the free blocks tree
}

/*
 * Set free blocks tree for arena
 * Updates the pointer to the root of the free blocks tree in the arena's as.self.free_blocks field
 */
static inline void arena_set_free_blocks(Arena *arena, Block *block) {
    ARENA_ASSERT((arena != NULL) && "Internal Error: 'arena_set_free_blocks' called on NULL arena");

    arena->as.self.free_blocks = block; // Set pointer to the root of the free blocks tree
}



/*
 * Get capacity from arena
 * Extracts the capacity information stored in the arena's as.block_representation field
 */
static inline size_t arena_get_capacity(const Arena *arena) {
    ARENA_ASSERT(arena != NULL && "Internal Error: 'arena_get_capacity' called on NULL arena");

    /*
     * What is happening here?
     * By design, the Arena struct if fully ABI compatible with Block struct, 
     *  we can safely use dedicated Block functions by treating Arena as a Block.
    */

    return get_size(&(arena->as.block_representation));
}

/*
 * Set capacity for arena
 * Updates the capacity information in the arena's as.block_representation field
 */
static inline void arena_set_capacity(Arena *arena, size_t size) {
    ARENA_ASSERT((arena != NULL)                         && "Internal Error: 'arena_set_capacity' called on NULL arena");
    ARENA_ASSERT(((size == 0 || size >= BLOCK_MIN_SIZE)) && "Internal Error: 'arena_set_capacity' called on too small size");
    ARENA_ASSERT((size <= SIZE_MASK)                     && "Internal Error: 'arena_set_capacity' called on too big size");

    /*
     * What is happening here?
     * By design, the Arena struct if fully ABI compatible with Block struct, 
     *  we can safely use dedicated Block functions by treating Arena as a Block.
    */

    set_size(&(arena->as.block_representation), size);
}



/*
 * Get alignment from arena
 * Extracts the alignment information stored in the arena's as.block_representation field
 */
static inline size_t arena_get_alignment(const Arena *arena) {
    ARENA_ASSERT((arena != NULL) && "Internal Error: 'arena_get_alignment' called on NULL arena");

    /*
     * What is happening here?
     * By design, the Arena struct if fully ABI compatible with Block struct, 
     *  we can safely use dedicated Block functions by treating Arena as a Block.
    */

    return get_alignment(&(arena->as.block_representation));
}

/*
 * Set alignment for arena
 * Updates the alignment information in the arena's as.block_representation field
 */
static inline void arena_set_alignment(Arena *arena, size_t alignment) {
    ARENA_ASSERT((arena != NULL)                      && "Internal Error: 'arena_set_alignment' called on NULL arena");
    ARENA_ASSERT(((alignment & (alignment - 1)) == 0) && "Internal Error: 'arena_set_alignment' called on invalid alignment");
    ARENA_ASSERT((alignment >= MIN_ALIGNMENT)         && "Internal Error: 'arena_set_alignment' called on too small alignment");
    ARENA_ASSERT((alignment <= MAX_ALIGNMENT)         && "Internal Error: 'arena_set_alignment' called on too big alignment");
    
    /*
     * What is happening here?
     * By design, the Arena struct if fully ABI compatible with Block struct, 
     *  we can safely use dedicated Block functions by treating Arena as a Block.
    */

    set_alignment(&(arena->as.block_representation), alignment);
}



/*
 * Get first block in arena
 * Calculates the pointer to the first block in the arena based on its alignment
 */
static inline Block *arena_get_first_block(const Arena *arena) {
    ARENA_ASSERT((arena != NULL) && "Internal Error: 'arena_get_first_block' called on NULL arena");
    
    /*
     * What is happening here?
     * Since the Arena_c uses alignment for blocks, the first block may not be located immediately after the Arena struct.
     * For example(on 64-bit systems), if the arena alignment is 32 bytes, and the size of Arena struct is 32 bytes (4 machine words),
     *  malloc or stack allocation may return an address with 8 byte alignment, so to have the userdata of first block aligned to desired 32 bytes,
     *  we need to calculate padding after the Arena struct that, after adding Block size metadata, reaches the next 32-byte aligned address.
     * 
     * Arena does that calculation automatically while created, to ensure alignment requirements are met.
     * 
     * To find the first block, we need to calculate its address based on the arena's alignment.
    */

    size_t align = arena_get_alignment(arena); // Get arena alignment
    uintptr_t raw_start = (uintptr_t)arena + sizeof(Arena); // Calculate raw start address of the first block

    uintptr_t aligned_start = align_up(raw_start + sizeof(Block), align) - sizeof(Block); // Align the start address to the arena's alignment
    
    return (Block *)aligned_start;
} 





/*
 * Get arena from bump
 * Extracts the arena pointer stored in the block's as.occupied.arena field
 */
static inline Arena *bump_get_arena(const Bump *bump) {
    ARENA_ASSERT((bump != NULL) && "Internal Error: 'bump_get_arena' called on NULL bump");

    return get_arena(&(bump->as.block_representation)); // Return pointer to the parent arena
}

/*
 * Set arena for bump
 * Updates the arena pointer in the block's as.occupied.arena field
 */
static inline void bump_set_arena(Bump *bump, Arena *arena) {
    ARENA_ASSERT((bump != NULL)  && "Internal Error: 'bump_set_arena' called on NULL bump");
    ARENA_ASSERT((arena != NULL) && "Internal Error: 'bump_set_arena' called on NULL arena");

    set_arena(&(bump->as.block_representation), arena); // Set pointer to the parent arena;
}



/*
 * Get offset from bump
 * Extracts the offset stored in the bump's as.self.offset field
 */
static inline size_t bump_get_offset(const Bump *bump) {
    ARENA_ASSERT((bump != NULL) && "Internal Error: 'bump_get_offset' called on NULL bump");

    return bump->as.self.offset;
}

/*
 * Set offset for bump
 * Updates the offset in the bump's as.self.offset field
 */
static inline void bump_set_offset(Bump *bump, size_t offset) {
    ARENA_ASSERT((bump != NULL)  && "Internal Error: 'bump_set_offset' called on NULL bump");

    bump->as.self.offset = offset;
}



/*
 * Get capacity of Bump
 * Extracts the size information stored in the bump's as.block_representation field
 */
static inline size_t bump_get_capacity(const Bump *bump) {
    ARENA_ASSERT((bump != NULL) && "Internal Error: 'bump_get_capacity' called on NULL bump");

    return get_size(&(bump->as.block_representation));
}

/*
 * Set capacity for Bump
 * Updates the size information in the bump's as.block_representation field
 */
static inline void bump_set_capacity(Bump *bump, size_t size) {
    ARENA_ASSERT((bump != NULL)  && "Internal Error: 'bump_set_capacity' called on NULL bump");

    return set_size(&(bump->as.block_representation), size);
}





/*
 * Get free size in tail block of arena
 * Calculates the amount of free space available in the tail block of the arena
 */
static inline size_t free_size_in_tail(const Arena *arena) {
    ARENA_ASSERT((arena != NULL) && "Internal Error: 'free_size_in_tail' called on NULL arena");
   
    Block *tail = arena_get_tail(arena); // Get the tail block of the arena 
    if (!tail || !get_is_free(tail)) return 0;  // If tail is NULL or not free, that means no free space in tail

    size_t occupied_relative_to_arena = (uintptr_t)tail + sizeof(Block) + get_size(tail) - (uintptr_t)arena;
    
    return arena_get_capacity(arena) - occupied_relative_to_arena;
}

/*
 * Get next block from given block (unsafe)
 * Calculates the pointer to the next block based on the current block's size
 * Note: This function does not perform any boundary checks, it just does pointer arithmetic.
 */
static inline Block *next_block_unsafe(const Block *block) {
    ARENA_ASSERT((block != NULL) && "Internal Error: 'next_block_unsafe' called on NULL block");

    return (Block *)((uintptr_t)block_data(block) + get_size(block)); // Calculate next block address based on current block's size
}

/*
 * Check if block is within arena bounds
 * Verifies if the given block is within the arena's capacity
 */
static inline bool is_block_within_arena(const Arena *arena, const Block *block) {
    ARENA_ASSERT((arena != NULL) && "Internal Error: 'is_block_within_arena' called on NULL arena");
    ARENA_ASSERT((block != NULL) && "Internal Error: 'is_block_within_arena' called on NULL block");
 
    return ((uintptr_t)block >= (uintptr_t)arena_get_first_block(arena)) &&
           ((uintptr_t)block <  (uintptr_t)(arena) + arena_get_capacity(arena)); // Check if block address is within arena bounds
}

/*
 * Check if block is in active part of arena
 * Verifies if the given block is within the active part of the arena (not in free tail)
 */
static inline bool is_block_in_active_part(const Arena *arena, const Block *block) {
    ARENA_ASSERT((arena != NULL) && "Internal Error: 'is_block_in_active_part' called on NULL arena");
    ARENA_ASSERT((block != NULL) && "Internal Error: 'is_block_in_active_part' called on NULL block");
    
    if (!is_block_within_arena(arena, block)) return false;

    return ((uintptr_t)block <= (uintptr_t)arena_get_tail(arena));
}

/*
 * Get next block from given block (safe)
 * Calculates the pointer to the next block if it is within the arena bounds
 * Returns NULL if the next block is out of arena bounds
 */
static inline Block *next_block(const Arena *arena, const Block *block) {
    ARENA_ASSERT((arena != NULL) && "Internal Error: 'next_block' called on NULL arena");
    ARENA_ASSERT((block != NULL) && "Internal Error: 'next_block' called on NULL block");
    
    Block *next_block = next_block_unsafe(block);
    if (!is_block_in_active_part(arena, next_block)) return NULL; // Check if next block is within arena bounds

    return next_block;
}

/* 
 * Block creation functions
 * Functions to initialize new blocks within the arena
 */
static inline Block *create_block(void *point) {
    ARENA_ASSERT((point != NULL) && "Internal Error: 'create_block' called on NULL pointer");

    // Initialize block metadata
    Block *block = (Block *)point;
    set_size(block, 0);
    set_prev(block, NULL);
    set_is_free(block, true);
    set_color(block, RED);
    set_left_tree(block, NULL);
    set_right_tree(block, NULL);

    return block;
}

/*
 * Create next block in arena
 * Initializes a new block following the given previous block within the arena
 */
static inline Block *create_next_block(Arena *arena, Block *prev_block) {
    ARENA_ASSERT((arena != NULL)      && "Internal Error: 'create_next_block' called on NULL arena");
    ARENA_ASSERT((prev_block != NULL) && "Internal Error: 'create_next_block' called on NULL prev_block");
    
    Block *next_block = NULL;
    if (is_block_within_arena(arena, prev_block)) {
        next_block = next_block_unsafe(prev_block);
        
        // Safety check - next block already exists
        if (is_block_in_active_part(arena, next_block) && get_prev(next_block) == prev_block) return NULL;
    }
    // LCOV_EXCL_START
    else {
        // Safety check - prev_block is out of arena bounds
        ARENA_ASSERT(false && "Internal Error: 'create_next_block' called with prev_block out of arena bounds");
    }
    // LCOV_EXCL_STOP

    next_block = create_block(next_block);
    set_prev(next_block, prev_block);
    
    return next_block;
}

/*
 * Helper: Merge source into target
 * Source must be physically immediately after target.
 */
static inline void merge_blocks_logic(Arena *arena, Block *target, Block *source) {
    ARENA_ASSERT((arena != NULL)   && "Internal Error: 'merge_blocks_logic' called on NULL arena");
    ARENA_ASSERT((target != NULL)  && "Internal Error: 'merge_blocks_logic' called on NULL target");
    ARENA_ASSERT((source != NULL)  && "Internal Error: 'merge_blocks_logic' called on NULL source");
    ARENA_ASSERT((next_block_unsafe(target) == source) && "Internal Error: 'merge_blocks_logic' called with non-adjacent blocks");

    size_t new_size = get_size(target) + sizeof(Block) + get_size(source);
    set_size(target, new_size);

    Block *following = next_block(arena, target);
    if (following) {
        set_prev(following, target);
    }
}





/*
 * Rotate left
 * Used to balance the LLRB tree
 */
Block *rotateLeft(Block *current_block) {
    ARENA_ASSERT((current_block != NULL) && "Internal Error: 'rotateLeft' called on NULL current_block");
    
    Block *x = get_right_tree(current_block);
    set_right_tree(current_block, get_left_tree(x));
    set_left_tree(x, current_block);

    set_color(x, get_color(current_block));
    set_color(current_block, RED);

    return x;
}

/*
 * Rotate right
 * Used to balance the LLRB tree
 */
Block *rotateRight(Block *current_block) {
    ARENA_ASSERT((current_block != NULL) && "Internal Error: 'rotateRight' called on NULL current_block");
    
    Block *x = get_left_tree(current_block);
    set_left_tree(current_block, get_right_tree(x));
    set_right_tree(x, current_block);

    set_color(x, get_color(current_block));
    set_color(current_block, RED);

    return x;
}

/*
 * Flip colors
 * Used to balance the LLRB tree
 */
void flipColors(Block *current_block) {
    ARENA_ASSERT((current_block != NULL) && "Internal Error: 'flipColors' called on NULL current_block");
    
    set_color(current_block, RED);
    set_color(get_left_tree(current_block), BLACK);
    set_color(get_right_tree(current_block), BLACK);
}

/*
 * Check if block is red
 * Helper function to check if a block is red in the LLRB tree
 */
static inline bool is_red(Block *block) {
    if (block == NULL) return false;
    return get_color(block) == RED;
}

/*
 * Balance LLRB tree
 * Balances the LLRB tree after insertions or deletions
 */
static Block *balance(Block *h) {
    ARENA_ASSERT((h != NULL) && "Internal Error: 'balance' called on NULL block");

    if (is_red(get_right_tree(h))) 
        h = rotateLeft(h);
    
    if (is_red(get_left_tree(h)) && is_red(get_left_tree(get_left_tree(h)))) 
        h = rotateRight(h);
    
    if (is_red(get_left_tree(h)) && is_red(get_right_tree(h))) 
        flipColors(h);

    return h;
}

/*
 * Insert block into LLRB tree
 * Inserts a new free block into the LLRB tree based on size, alignment, and address
 */
static Block *insert_block(Block *h, Block *new_block) {
    ARENA_ASSERT((new_block != NULL) && "Internal Error: 'insert_block' called on NULL new_block");

    /*
     * Logic Overview:
     * This function utilizes a "Triple-Key" insertion strategy to keep the free-block tree
     * highly optimized for subsequent "Best-Fit" searches:
     * 
     * 1. Primary Key: Size. 
     *    The tree is sorted primarily by block size. This allows the allocator to find 
     *    a block that fits the requested size in O(log n) time.
     * 
     * 2. Secondary Key: Alignment Quality (CTZ - Count Trailing Zeros).
     *    If two blocks have the same size, we sort them by the "quality" of their data pointer 
     *    alignment. Blocks with higher alignment (more trailing zeros) are placed in the 
     *    right sub-tree. This clusters "cleaner" addresses together, helping the search 
     *    algorithm find high-alignment blocks (e.g., 64-byte aligned) faster.
     * 
     * 3. Tertiary Key: Raw Address.
     *    If both size and alignment quality are identical, the raw memory address is used 
     *    as a final tie-breaker to ensure every node in the tree is unique and the 
     *    ordering is deterministic.
    */

    if (h == NULL) return new_block;

    size_t h_size = get_size(h);
    size_t new_size = get_size(new_block);

    if (new_size < h_size) {
        set_left_tree(h, insert_block(get_left_tree(h), new_block));
    } 
    else if (new_size > h_size) {
        set_right_tree(h, insert_block(get_right_tree(h), new_block));
    } 
    else {
        size_t h_quality = min_exponent_of((uintptr_t)block_data(h));
        size_t new_quality = min_exponent_of((uintptr_t)block_data(new_block));

        if (new_quality < h_quality) {
            set_left_tree(h, insert_block(get_left_tree(h), new_block));
        }
        else if (new_quality > h_quality) {
            set_right_tree(h, insert_block(get_right_tree(h), new_block));
        }
        else {
            if ((uintptr_t)new_block > (uintptr_t)h)
                set_left_tree(h, insert_block(get_left_tree(h), new_block));
            else
                set_right_tree(h, insert_block(get_right_tree(h), new_block));
        }
    }

    return balance(h);
}

/*
 * Find best fit block in LLRB tree
 * Searches the LLRB tree for the best fitting free block that can accommodate the requested size and alignment
 *
 * Strategy: 
 *   The tree is ordered primarily by size, and secondarily by "address quality" (CTZ).
 *   We aim to find the smallest block that satisfies: block_size >= requested_size + alignment_padding.
 *   Performance: O(log n)
 */
static Block *find_best_fit(Block *root, size_t size, size_t alignment, Block **out_parent) {
    ARENA_ASSERT((size > 0)          && "Internal Error: 'find_best_fit' called on too small size");
    ARENA_ASSERT((size <= SIZE_MASK) && "Internal Error: 'find_best_fit' called on too big size");
    ARENA_ASSERT(((alignment & (alignment - 1)) == 0) && "Internal Error: 'find_best_fit' called on invalid alignment");
    ARENA_ASSERT((alignment >= MIN_ALIGNMENT)         && "Internal Error: 'find_best_fit' called on too small alignment");
    ARENA_ASSERT((alignment <= MAX_ALIGNMENT)         && "Internal Error: 'find_best_fit' called on too big alignment");
    
    if (root == NULL) return NULL;

    Block *best = NULL;
    Block *best_parent = NULL;
    Block *current = root;
    Block *current_parent = NULL;

    while (current != NULL) {
        size_t current_size = get_size(current);
        
        /* 
         * CASE 1: Block is physically too small.
         * Since the tree is sorted by size (left < current < right), 
         * all blocks in the left sub-tree are guaranteed to be even smaller.
         * We MUST search the right sub-tree.
        */
        if (current_size < size) {
            current_parent = current;
            current = get_right_tree(current);
            continue;
        }

        uintptr_t data_ptr = (uintptr_t)block_data(current);
        uintptr_t aligned_ptr = align_up(data_ptr, alignment);
        size_t padding = aligned_ptr - data_ptr;

        /* 
         * CASE 2: Block is large enough to fit size + padding.
         * It is a valid candidate. We record it and then try to find an even 
         * smaller (better) block in the left sub-tree.
        */
        if (current_size >= size + padding) {
            // Potential best fit found. 
            // We keep the smallest block that can satisfy the request.
            if (best == NULL || current_size < get_size(best)) {
                best_parent = current_parent;
                best = current;
            }

            // Look for a smaller block in the left sub-tree.
            current_parent = current;
            current = get_left_tree(current);
        }

        /* 
         * CASE 3: Block is large enough on its own, but insufficient after padding.
         * This means the address of this block is "poorly aligned" for the request.
         * Since our tree sorts same-sized blocks by "address quality" (right has more trailing zeros),
         * we go RIGHT to find a block of the same or larger size with better alignment properties.
        */
        else {
            current_parent = current;
            current = get_right_tree(current);
        }
    }

    if (out_parent) *out_parent = best_parent;
    return best;
}

/*
 * Detach block from LLRB tree (fast version)
 * Removes a block from the LLRB tree without rebalancing
 * Note: Uses pragmatic BST deletion with a single balance pass at the root.
 */
static void detach_block_fast(Block **tree_root, Block *target, Block *parent) {
    ARENA_ASSERT((tree_root != NULL) && "Internal Error: 'detach_block_fast' called on NULL tree_root");
    ARENA_ASSERT((target != NULL) && "Internal Error: 'detach_block_fast' called on NULL target");

    Block *replacement = NULL;
    Block *left_child = get_left_tree(target);
    Block *right_child = get_right_tree(target);

    if (!right_child) {
        replacement = left_child;
    } else if (!left_child) {
        replacement = right_child;
    } else {
        Block *min_parent = target;
        Block *min_node = right_child;
        while (get_left_tree(min_node)) {
            min_parent = min_node;
            min_node = get_left_tree(min_node);
        }
        if (min_parent != target) {
            set_left_tree(min_parent, get_right_tree(min_node));
            set_right_tree(min_node, right_child);
        }
        set_left_tree(min_node, left_child);
        replacement = min_node;
    }

    if (parent == NULL) {
        *tree_root = replacement;
    } else {
        if (get_left_tree(parent) == target)
            set_left_tree(parent, replacement);
        else
            set_right_tree(parent, replacement);
    }

    set_left_tree(target, NULL);
    set_right_tree(target, NULL);
    set_color(target, RED);
    
    if (*tree_root) *tree_root = balance(*tree_root);
}

/*
 * Find and detach block
 * High-level internal function that searches for the best fit and removes it from the tree.
 * Returns the detached block or NULL if no suitable block was found.
 */
static Block *find_and_detach_block(Block **tree_root, size_t size, size_t alignment) {
    ARENA_ASSERT((size > 0)          && "Internal Error: 'find_and_detach_block' called on too small size");
    ARENA_ASSERT((size <= SIZE_MASK) && "Internal Error: 'find_and_detach_block' called on too big size");
    ARENA_ASSERT(((alignment & (alignment - 1)) == 0) && "Internal Error: 'find_and_detach_block' called on invalid alignment");
    ARENA_ASSERT((alignment >= MIN_ALIGNMENT)         && "Internal Error: 'find_and_detach_block' called on too small alignment");
    ARENA_ASSERT((alignment <= MAX_ALIGNMENT)         && "Internal Error: 'find_and_detach_block' called on too big alignment");
    
    if (*tree_root == NULL) return NULL;

    Block *parent = NULL;
    Block *best = find_best_fit(*tree_root, size, alignment, &parent);

    if (best) {
        detach_block_fast(tree_root, best, parent);
    }

    return best;
}

/*
 * Detach a specific block by its pointer
 * Finds the parent of the given block using Triple-Key logic and detaches it.
 */
static void detach_block_by_ptr(Block **tree_root, Block *target) {
    ARENA_ASSERT((tree_root != NULL) && "Internal Error: 'detach_block_by_ptr' called on NULL tree_root");
    ARENA_ASSERT((target != NULL) && "Internal Error: 'detach_block_by_ptr' called on NULL target");

    Block *parent = NULL;
    Block *current = *tree_root;

    size_t target_size = get_size(target);
    size_t target_quality = min_exponent_of((uintptr_t)block_data(target));

    while (current != NULL && current != target) {
        parent = current;
        size_t current_size = get_size(current);

        if (target_size < current_size) {
            current = get_left_tree(current);
        } else if (target_size > current_size) {
            current = get_right_tree(current);
        } else {
            size_t current_quality = min_exponent_of((uintptr_t)block_data(current));
            if (target_quality < current_quality) {
                current = get_left_tree(current);
            } else if (target_quality > current_quality) {
                current = get_right_tree(current);
            } else {
                if ((uintptr_t)target > (uintptr_t)current)
                    current = get_left_tree(current);
                else
                    current = get_right_tree(current);
            }
        }
    }

    if (current == target) {
        detach_block_fast(tree_root, target, parent);
    }
}

static void arena_free_block_full(Arena *arena, Block *block);
/*
 * Split block
 * Splits a larger free block into two blocks if it is significantly larger than needed
 * The remainder block is added back to the free blocks tree
 */
static inline void split_block(Arena *arena, Block *block, size_t needed_size) {
    size_t full_size = get_size(block);
    
    if (full_size > needed_size && full_size - needed_size >= BLOCK_MIN_SIZE) {
        set_size(block, needed_size);

        Block *remainder = create_block(next_block_unsafe(block)); 
        set_prev(remainder, block);
        set_size(remainder, full_size - needed_size - sizeof(Block));
        
        Block *following = next_block(arena, remainder);
        if (following) {
            set_prev(following, remainder);
        }

        arena_free_block_full(arena, remainder);
    }
}

/*
 * Internal: Get the arena that owns this block
 * Uses neighbor-borrowing or the LSB Padding Detector to find the header.
 */
static inline Arena *get_parent_arena(Block *block) {
    Block *prev = block;
    
    /*
     * Logic: Physical Neighbor Walkback
     * We traverse the 'prev' pointers, which point to physical neighbors in memory.
     * We are looking for a block that can tell us who its owner is.
    */
    while (get_prev(prev) != NULL) {
        prev = get_prev(prev);

        /* 
         * We found an occupied block. But wait!
         * Because Arena and Block are ABI-compatible, a nested arena 
         * LOOKS like an occupied block to its parent. 
         * We must check the 'IS_NESTED' flag to ensure we don't accidentally 
         * treat a nested arena as a simple block.
        */
        if (!get_is_free(prev) && !arena_get_is_nested((Arena*)(void *)(prev))) {
            return get_arena(prev);
        }

        // If it's a nested arena or a free block, we keep walking back.
    }

    /*
     * Logic: Terminal Case - The First Block
     * If we reach the start (prev == NULL), we are at the very beginning of 
     * the arena segment. We check the word immediately preceding the block.
     * To get more understanding whats going on go to 'arena_new_static_custom'
     * function. 
    */
    uintptr_t *detector_spot = (uintptr_t *)((char *)block - sizeof(uintptr_t));
    uintptr_t val = *detector_spot;
    
    if (val & 1) return (Arena *)((char *)prev - (val >> 1));

    return (Arena *)((char *)prev - sizeof(Arena));
}





/*
 * Free block (full version)
 * Frees a block of memory and merges with adjacent free blocks if possible
 */
static void arena_free_block_full(Arena *arena, Block *block) {
    ARENA_ASSERT((arena != NULL) && "Internal Error: 'arena_free_block_full' called on NULL arena");
    ARENA_ASSERT((block != NULL) && "Internal Error: 'arena_free_block_full' called on NULL block");

    set_is_free(block, true);
    set_left_tree(block, NULL);
    set_right_tree(block, NULL);
    set_color(block, RED);

    Block *tail = arena_get_tail(arena);
    Block *prev = get_prev(block);
    
    Block *result_to_tree = block;
    
    // If block is tail, just set its size to 0
    if (block == tail) {
        set_size(block, 0);
        result_to_tree = NULL;
    }
    else {
        Block *next = next_block(arena, block);

        // If next block is tail, just set its size to 0 and update tail pointer
        if (next == tail) {
            set_size(block, 0);
            arena_set_tail(arena, block);
            result_to_tree = NULL; 
        } 
        // Merge with next block if it is free
        else if (next && get_is_free(next)) {
            Block *free_blocks_root = arena_get_free_blocks(arena);
            detach_block_by_ptr(&free_blocks_root, next);
            arena_set_free_blocks(arena, free_blocks_root);
            merge_blocks_logic(arena, block, next);
            result_to_tree = block;
        }
    }

    // Merge with previous block if it is free
    if (prev && get_is_free(prev)) {
        Block *free_blocks_root = arena_get_free_blocks(arena);
        detach_block_by_ptr(&free_blocks_root, prev);
        arena_set_free_blocks(arena, free_blocks_root);

        // If we merged with tail before, just update tail pointer
        if (result_to_tree == NULL) {
            set_size(prev, 0);
            arena_set_tail(arena, prev);
        } 
        // Else, merge previous with current result
        else {
            merge_blocks_logic(arena, prev, result_to_tree);
            result_to_tree = prev;
        }
    }

    // Insert the resulting free block back into the free blocks tree
    if (result_to_tree != NULL) {
        Block *free_blocks_root = arena_get_free_blocks(arena);
        free_blocks_root = insert_block(free_blocks_root, result_to_tree);
        arena_set_free_blocks(arena, free_blocks_root);
    }
}

/*
 * Allocate memory in free blocks of arena
 * Attempts to allocate a block of memory of given size and alignment from the free blocks tree of the arena
 * Returns pointer to allocated memory or NULL if allocation fails
 */
static void *alloc_in_free_blocks(Arena *arena, size_t size, size_t alignment) {
    ARENA_ASSERT((arena != NULL)                      && "Internal Error: 'alloc_in_free_blocks' called on NULL arena");
    ARENA_ASSERT((size > 0)                           && "Internal Error: 'alloc_in_free_blocks' called on too small size");
    ARENA_ASSERT((size <= SIZE_MASK)                  && "Internal Error: 'alloc_in_free_blocks' called on too big size");
    ARENA_ASSERT(((alignment & (alignment - 1)) == 0) && "Internal Error: 'alloc_in_free_blocks' called on invalid alignment");
    ARENA_ASSERT((alignment >= MIN_ALIGNMENT)         && "Internal Error: 'alloc_in_free_blocks' called on too small alignment");
    ARENA_ASSERT((alignment <= MAX_ALIGNMENT)         && "Internal Error: 'alloc_in_free_blocks' called on too big alignment");

    Block *root = arena_get_free_blocks(arena);
    Block *block = find_and_detach_block(&root, size, alignment);
    arena_set_free_blocks(arena, root);
    
    if (!block) return NULL;
    
    set_is_free(block, false);
    
    uintptr_t data_ptr = (uintptr_t)block_data(block);
    uintptr_t aligned_ptr = align_up(data_ptr, alignment);
    size_t padding = aligned_ptr - data_ptr;

    size_t total_needed = padding + size;
    size_t aligned_needed = align_up(total_needed, sizeof(uintptr_t)); 
    
    split_block(arena, block, aligned_needed);

    if (padding > 0) {
        uintptr_t *spot_before = (uintptr_t *)(aligned_ptr - sizeof(uintptr_t));
        *spot_before = (uintptr_t)block ^ aligned_ptr;
    }

    set_arena(block, arena);
    set_magic(block, (void *)aligned_ptr);

    return (void *)aligned_ptr;
}

/*
 * Allocate memory in tail block of arena (full version)
 * Attempts to allocate a block of memory of given size and alignment in the tail block of the arena
 * Returns pointer to allocated memory or NULL if allocation fails
 */
static void *alloc_in_tail_full(Arena *arena, size_t size, size_t alignment) {
    ARENA_ASSERT((arena != NULL)                      && "Internal Error: 'alloc_in_tail_full' called on NULL arena");
    ARENA_ASSERT((size > 0)                           && "Internal Error: 'alloc_in_tail_full' called on too small size");
    ARENA_ASSERT((size <= SIZE_MASK)                  && "Internal Error: 'alloc_in_tail_full' called on too big size");
    ARENA_ASSERT(((alignment & (alignment - 1)) == 0) && "Internal Error: 'alloc_in_tail_full' called on invalid alignment");
    ARENA_ASSERT((alignment >= MIN_ALIGNMENT)         && "Internal Error: 'alloc_in_tail_full' called on too small alignment");
    ARENA_ASSERT((alignment <= MAX_ALIGNMENT)         && "Internal Error: 'alloc_in_tail_full' called on too big alignment");
    if (free_size_in_tail(arena) < size) return NULL;  // Quick check to avoid unnecessary calculations
    
    /*
     * Allocation in tail may seem simple at first glance, but there are several edge cases to consider:
     * 1. Alignment padding before user data:
     *      If the required alignment is greater than the arena's alignment, 
     *       we need to calculate the padding needed before the user data to satisfy the alignment requirement.
     *      If calculated padding is so big that it by itself can contain a whole minimal block(BLOCK_MIN_SIZE) or more,
     *       we need to create the new block. It will allow us to reuse that, in other case wasted, memory if needed.
     * 2. Alignment padding after user data:
     *      After allocating the requested size, we need to check if there is enough space left in the tail block to create a new free block.
     *      This may require additional alignment padding after the user data to ensure that the data pointer of the next block 
     *       is aligned according to the arena alignment.
     * 3. Minimum block size:
     *      We need to ensure that any new blocks created (either before or after the user data) meet the minimum block size requirement.
     * 4. Insufficient space:
     *      If there is not enough space in the tail block to satisfy the allocation request (including any necessary padding), we must return NULL.
    */

    Block *tail = arena_get_tail(arena);
    ARENA_ASSERT((tail != NULL)      && "Internal Error: alloc_in_tail_full' called on NULL tail");
    ARENA_ASSERT((get_is_free(tail)) && "Internal Error: alloc_in_tail_full' called on non free tail");

    // Calculate padding needed for alignment before user data
    uintptr_t raw_data_ptr = (uintptr_t)block_data(tail);
    uintptr_t aligned_data_ptr = align_up(raw_data_ptr, alignment);
    size_t padding = aligned_data_ptr - raw_data_ptr;

    size_t minimal_needed_block_size = padding + size;

    size_t free_space = free_size_in_tail(arena);
    if (minimal_needed_block_size > free_space) return NULL;

    // If alignment padding is bigger than arena alignment, 
    //  it may be possible to create a new block before user data
    if (alignment > arena_get_alignment(arena) && padding > 0) {
        if (padding >= BLOCK_MIN_SIZE) {
            set_size(tail, padding - sizeof(Block));
            Block *free_blocks_root = arena_get_free_blocks(arena);
            free_blocks_root = insert_block(free_blocks_root, tail);
            arena_set_free_blocks(arena, free_blocks_root);

            Block *new_tail = create_next_block(arena, tail);
            arena_set_tail(arena, new_tail);
            tail = new_tail;
            padding = 0;
        }
    }

    minimal_needed_block_size = padding + size;

    free_space = free_size_in_tail(arena);
    if (minimal_needed_block_size > free_space) return NULL;

    // Check if we can allocate with end padding for next block
    size_t final_needed_block_size = minimal_needed_block_size;
    if (free_space - minimal_needed_block_size >= BLOCK_MIN_SIZE) {
        uintptr_t raw_data_end_ptr = aligned_data_ptr + size;
        uintptr_t aligned_data_end_ptr = align_up(raw_data_end_ptr + sizeof(Block), arena_get_alignment(arena)) - sizeof(Block);
        size_t end_padding = aligned_data_end_ptr - raw_data_end_ptr;
    
        size_t full_needed_block_size = minimal_needed_block_size + end_padding;
        if (free_space - full_needed_block_size >= BLOCK_MIN_SIZE) {
            final_needed_block_size = full_needed_block_size;
        } else {
            final_needed_block_size = free_space;
        }
    } else {
        final_needed_block_size = free_space;
    } 
    
    /*
    * Why we sure that padding >= sizeof(uintptr_t) here?
    * 
    * Since we are allocating aligned memory, the alignment is always a power of two and at least sizeof(uintptr_t).
    * Therefore, any padding in 'padding' variable will be always 0 or powers of 2 with sizeof(uintptr_t) as minimum.
    */
   
    // Store pointer to block metadata before user data for deallocation if there is padding
    if (padding > 0) {
        uintptr_t *spot_before_user_data = (uintptr_t *)(aligned_data_ptr - sizeof(uintptr_t));
        *spot_before_user_data = (uintptr_t)tail ^ aligned_data_ptr;
    }

    // Finalize tail block as occupied
    set_size(tail, final_needed_block_size);
    set_is_free(tail, false);
    set_magic(tail, (void *)aligned_data_ptr);
    set_arena(tail, arena);

    // If there is remaining free space, create a new free block
    if (free_space != final_needed_block_size) {
        Block *new_tail = create_next_block(arena, tail);
        arena_set_tail(arena, new_tail);
    }

    return (void *)aligned_data_ptr;
}





/*
 * Free a block of memory in the arena
 * Marks the block as free, merges it with adjacent free blocks if possible,
 * and updates the free block list
 */
void arena_free_block(void *data) {
    if (!data) return;
    if ((uintptr_t)data % sizeof(uintptr_t) != 0) return;

    Block *block = NULL;

    /*
     * Retrieve block metadata from user data pointer
     * We have two possible scenarios:
     * 1. There is no alignment padding before user data:
     *      In this case, we can directly calculate the block pointer by subtracting the size of Block
     *      from the user data pointer.
     * 2. There is alignment padding before user data:
     *      In this case, we stored the block pointer XORed with user data pointer just before the user data.
     *      We can retrieve it and XOR it back with user data pointer to get the original block pointer.
     * 
     * Thanks to Block struct having magic value as last field, we can validate which scenario is correct by just checking
     *  whether the XORed value before the user data matches the expected value.
    */

    uintptr_t *spot_before_user_data = (uintptr_t *)((char *)data - sizeof(uintptr_t));
    uintptr_t check = *spot_before_user_data ^ (uintptr_t)data;
    if (check == (uintptr_t)0xDEADBEEF) {
        block = (Block *)(void *)((char *)data - sizeof(Block));
    }
    else {
        if ((uintptr_t)check % sizeof(uintptr_t) != 0) return;
        block = (Block *)check;
    }
    
    ARENA_ASSERT((block != NULL) && "Internal Error: 'arena_free_block' detected NULL block");
    
    // If block size is bigger than SIZE_MASK, it's invalid
    if (get_size(block) > SIZE_MASK) return;
    // If block is already free, it's invalid
    if (get_is_free(block)) return;
    // If magic is invalid, it's invalid
    if (!is_valid_magic(block, data)) return;
    
    Arena *arena = get_arena(block);
    
    ARENA_ASSERT((arena != NULL) && "Internal Error: 'arena_free_block' detected block with NULL arena");
    
    // If block is out of arena bounds, it's invalid
    if (!is_block_within_arena(arena, block)) return;

    #ifdef ARENA_POISONING
    memset(block_data(block), ARENA_POISON_BYTE, get_size(block));
    #endif

    arena_free_block_full(arena, block);
}

/*
 * Allocate memory in the arena with custom alignment
 * Returns NULL if there is not enough space
 */
void *arena_alloc_custom(Arena *arena, size_t size, size_t alignment) {
    if (!arena || size == 0 || size > arena_get_capacity(arena)) return NULL;
    if ((alignment & (alignment - 1)) != 0) return NULL;
    if (alignment < MIN_ALIGNMENT || alignment > MAX_ALIGNMENT) return NULL;

    // Trying to allocate in free blocks first
    void *result = alloc_in_free_blocks(arena, size, alignment);
    if (result) return result;

    if (free_size_in_tail(arena) == 0) return NULL;
    return alloc_in_tail_full(arena, size, alignment);
}

/*
 * Allocate memory in the arena with default alignment
 * Returns NULL if there is not enough space
 */
void *arena_alloc(Arena *arena, size_t size) {
    if (!arena) return NULL;
    return arena_alloc_custom(arena, size, arena_get_alignment(arena));
}

/*
 * Allocate zero-initialized memory in the arena
 * Returns NULL if there is not enough space or overflow is detected
 */
void *arena_calloc(Arena *arena, size_t nmemb, size_t size) {
    if (!arena) return NULL;

    if (nmemb > 0 && (SIZE_MAX / nmemb) < size) {
        return NULL; // Overflow detected
    }

    size_t total_size = nmemb * size;
    void *ptr = arena_alloc(arena, total_size);
    if (ptr) {
        memset(ptr, 0, total_size); // Zero-initialize the allocated memory
    }
    return ptr;
}

/*
 * Create a static arena
 * Initializes an arena using preallocated memory and sets up the first block
 * Returns NULL if the provided size is too small, memory is NULL or size is negative
 */
Arena *arena_new_static_custom(void *memory, size_t size, size_t alignment) {
    if (!memory || size < ARENA_MIN_SIZE || size > SIZE_MASK) return NULL;
    if ((alignment & (alignment - 1)) != 0) return NULL;
    if (alignment < MIN_ALIGNMENT|| alignment > MAX_ALIGNMENT) return NULL;

    uintptr_t raw_addr = (uintptr_t)memory;
    uintptr_t aligned_addr = align_up(raw_addr, MIN_ALIGNMENT);
    size_t arena_padding = aligned_addr - raw_addr; 

    if (size < arena_padding + sizeof(Arena) + BLOCK_MIN_SIZE) return NULL;
    
    Arena *arena = (Arena *)aligned_addr;

    /*
     * Magic LSB Padding Detector
     *
     *What is this for?
     * One of the core goals of Arena_c is Zero-Cost Parent Tracking. We need to find 
     * the 'Arena' header starting from a 'Block' pointer (especially the first block) 
     * without storing an explicit 8-byte 'parent' pointer in every single block.
     *
     * In a nested or static arena, there is often a gap (padding) between the 
     * end of the 'Arena' struct and the start of the first 'Block' due to 
     * alignment requirements. Instead of wasting this space, we use it to store 
     * a "back-link" offset to the Arena header.
     *
     * Why/How are we sure there is enough space?
     * 1. Structural Invariants: Both 'Arena' and 'Block' structures are designed 
     *    to be multiples of the machine word (sizeof(uintptr_t)).
     * 2. Alignment Logic: Since 'alignment' is a power of two and is at least 
     *    sizeof(uintptr_t), any gap created by 'align_up' will also be 
     *    a multiple of the machine word.
     * 3. The Condition: If 'aligned_block_start' is greater than the end of the 
     *    Arena structure, the difference is guaranteed to be at least 4 bytes 
     *    (on 32-bit) or 8 bytes (on 64-bit). This is exactly the space needed 
     *    to store our tagged uintptr_t offset.
     *
     * The Detection Trick
     * We store the offset shifted left by 1, with the Least Significant Bit (LSB) 
     * set to 1 (e.g., (offset << 1) | 1). 
     * Why? Because the last field of an 'Arena' struct is 'free_blocks' (a pointer). 
     * Valid pointers are always word-aligned (even numbers). By checking the LSB, 
     * we can instantly distinguish between:
     *   - 0: We are looking at the 'free_blocks' pointer (Arena is immediately adjacent).
     *   - 1: We are looking at our custom padding offset (Arena is 'offset' bytes away).
    */

    uintptr_t aligned_block_start = align_up(aligned_addr + sizeof(Block) + sizeof(Arena), alignment) - sizeof(Block);
    Block *block = create_block((void *)(aligned_block_start));

    if (aligned_block_start > (aligned_addr + sizeof(Arena))) {
        uintptr_t offset = aligned_block_start - (uintptr_t)arena;
        uintptr_t *detector_spot = (uintptr_t *)(aligned_block_start - sizeof(uintptr_t));
        *detector_spot = (offset << 1) | 1;
    }

    arena_set_alignment(arena, alignment);
    arena_set_capacity(arena, size - arena_padding);
    arena_set_free_blocks(arena, NULL);
    arena_set_tail(arena, block);
    arena_set_is_dynamic(arena, false);

    return arena;
}

/*
 * Create a static arena with default alignment
 * Initializes an arena using preallocated memory with default alignment
 * Returns NULL if the provided size is too small, memory is NULL or size is negative
 */
Arena *arena_new_static(void *memory, size_t size) {
    return arena_new_static_custom(memory, size, ARENA_DEFAULT_ALIGNMENT);
}

#ifndef ARENA_NO_MALLOC
/*
 * Create a dynamic arena
 * Allocates memory for the arena and initializes it with the specified size and alignment
 * Returns NULL if the provided size is too small, memory allocation fails or size is negative
 */
Arena *arena_new_dynamic_custom(size_t size, size_t alignment) {
    if (size < BLOCK_MIN_SIZE || size > SIZE_MASK) return NULL;
    if ((alignment & (alignment - 1)) != 0) return NULL;
    if (alignment < MIN_ALIGNMENT|| alignment > MAX_ALIGNMENT) return NULL;

    void *data = malloc(size + sizeof(Arena) + alignment);
    if (!data) return NULL;
    
    Arena *arena = arena_new_static_custom(data, size + sizeof(Arena), alignment);

    if (!arena) {
        // LCOV_EXCL_START
        free(data);
        return NULL;
        // LCOV_EXCL_STOP
    }

    arena_set_is_dynamic(arena, true);

    return arena;
}

/*
 * Create a dynamic arena with default alignment
 * Allocates memory for the arena and initializes it with the specified size and default alignment
 * Returns NULL if the provided size is too small, memory allocation fails or size is negative
 */
Arena *arena_new_dynamic(size_t size) {
    return arena_new_dynamic_custom(size, ARENA_DEFAULT_ALIGNMENT);
}
#endif // ARENA_NO_MALLOC

/*
 * Free arena
 * Deallocates the memory used by the arena if it was dynamically allocated
 * Can be safely called with static arenas (no operation in that case)
 */
void arena_free(Arena *arena) {
    if (!arena) return;

    if (arena_get_is_nested(arena)) {
        Arena *parent = get_parent_arena((Block *)arena);
        arena_free_block_full(parent, (Block *)arena); 
        return;
    }

    #ifndef ARENA_NO_MALLOC
    if (arena_get_is_dynamic(arena)) {
        free(arena);
    }
    #endif // ARENA_NO_MALLOC
}

/*
 * Reset the arena
 * Clears the arena's blocks and resets it to the initial state without freeing memory
 */
void arena_reset(Arena *arena) {
    if (!arena) return;

    Block *first_block = arena_get_first_block(arena);

    // Reset first block
    set_size(first_block, 0);
    set_prev(first_block, NULL);
    set_is_free(first_block, true);
    set_color(first_block, RED);
    set_left_tree(first_block, NULL);
    set_right_tree(first_block, NULL);

    // Reset arena metadata
    arena_set_free_blocks(arena, NULL);
    arena_set_tail(arena, first_block);
}

/*
 * Reset the arena and set its tail to zero
 * clears the arens`s blocks and reserts it ti the initial state with zeroing all the memory
 */
void arena_reset_zero(Arena *arena) {
    if (!arena) return;
    arena_reset(arena); // Reset arena
    memset(block_data(arena_get_tail(arena)), 0, free_size_in_tail(arena)); // Set tail to zero
}

/*
 * Create a nested arena with custom alignment
 * Allocates memory for a nested arena from a parent arena and initializes it
 * Returns NULL if the parent arena is NULL, requested size is too small, or allocation fails
 */
Arena *arena_new_nested_custom(Arena *parent_arena, size_t size, size_t alignment) {
    if (!parent_arena || size < BLOCK_MIN_SIZE || size > SIZE_MASK) return NULL;
    if ((alignment & (alignment - 1)) != 0) return NULL;
    if (alignment < MIN_ALIGNMENT|| alignment > MAX_ALIGNMENT) return NULL;
    
    void *data = arena_alloc(parent_arena, size);  // Allocate memory from the parent arena
    if (!data) return NULL;

    Block *block = NULL;

    uintptr_t *spot_before_user_data = (uintptr_t *)((char *)data - sizeof(uintptr_t));
    uintptr_t check = *spot_before_user_data ^ (uintptr_t)data;
    if (check == (uintptr_t)0xDEADBEEF) {
        block = (Block *)(void *)((char *)data - sizeof(Block));
    }
    // LCOV_EXCL_START
    else {
        block = (Block *)check;
    }
    // LCOV_EXCL_STOP

    Arena *arena = arena_new_static_custom((void *)block, size, alignment);
    arena_set_is_nested(arena, true); // Mark the arena as nested

    return arena;
}

/*
 * Create a nested arena with alignment of parent arena
 * Allocates memory for a nested arena from a parent arena and initializes it
 * Returns NULL if the parent arena is NULL, requested size is too small, or allocation fails
 */
Arena *arena_new_nested(Arena *parent_arena, size_t size) {
    if (!parent_arena || size < BLOCK_MIN_SIZE || size > SIZE_MASK) return NULL;

    return arena_new_nested_custom(parent_arena, size, arena_get_alignment(parent_arena));
}


/*
 * Create a bump allocator
 * Initializes a bump allocator within a parent arena
 * Returns NULL if the parent arena is NULL, requested size is too small, or allocation fails
 */
Bump *bump_new(Arena *parent_arena, size_t size) {
    if (!parent_arena) return NULL;
    if (size > SIZE_MASK || size < ARENA_MIN_BUFFER_SIZE) return NULL;  // Check for minimal reasonable size
    
    void *data = arena_alloc(parent_arena, size);  // Allocate memory from the parent arena
    if (!data) return NULL;

    Block *block = NULL;

    uintptr_t *spot_before_user_data = (uintptr_t *)((char *)data - sizeof(uintptr_t));
    uintptr_t check = *spot_before_user_data ^ (uintptr_t)data;
    if (check == (uintptr_t)0xDEADBEEF) {
        block = (Block *)(void *)((char *)data - sizeof(Block));
    }
    // LCOV_EXCL_START
    else {
        block = (Block *)check;
    }
    // LCOV_EXCL_STOP
    
    Bump *bump = (Bump *)((void *)block);  // just cast allocated Block to Bump

    bump_set_arena(bump, parent_arena);
    bump_set_offset(bump, sizeof(Bump));

    return bump;
}

/*
 * Allocate memory from a bump allocator
 * Returns a pointer to the allocated memory or NULL if allocation fails
 * May return NOT aligned pointer
 */
void *bump_alloc(Bump *bump, size_t size) {
    if (!bump) return NULL;
    
    size_t offset = bump_get_offset(bump);
    if (size == 0 || size >= (bump_get_capacity(bump) - offset + sizeof(Bump))) return NULL;

    void *memory = (char *)bump + offset;
    bump_set_offset(bump, offset + size);

    return memory;
}

/*
 * Allocate aligned memory from a bump allocator
 * Returns a pointer to the allocated memory or NULL if allocation fails
 */
void *bump_alloc_aligned(Bump *bump, size_t size, size_t alignment) {
    if (!bump) return NULL;
    if ((alignment & (alignment - 1)) != 0) return NULL;
    if (alignment < MIN_ALIGNMENT|| alignment > MAX_ALIGNMENT) return NULL;
    if (size == 0) return NULL;

    uintptr_t current_ptr = (uintptr_t)bump + bump_get_offset(bump);
    uintptr_t aligned_ptr = align_up(current_ptr, alignment);
    size_t padding = aligned_ptr - current_ptr;

    if ((size_t)size > SIZE_MAX - padding) return NULL;

    size_t total_size = padding + size;

    size_t offset = bump_get_offset(bump);
    if ((size_t)total_size >= (bump_get_capacity(bump) - offset + sizeof(Bump))) return NULL;

    bump_set_offset(bump, offset + total_size);

    return (void *)aligned_ptr;
}

/*
 * Trim a bump allocator
 * Trims the bump allocator and return free part back to arena
 */
void bump_trim(Bump *bump) {
    if (!bump) return;

    Arena *parent = bump_get_arena(bump);
    size_t parent_align = arena_get_alignment(parent);
    uintptr_t bump_addr = (uintptr_t)bump;
    
    uintptr_t current_end = bump_addr + bump_get_offset(bump);
    uintptr_t next_data_aligned = align_up(current_end + sizeof(Block), parent_align);

    uintptr_t remainder_addr = next_data_aligned - sizeof(Block);

    size_t new_payload_size = remainder_addr - ((uintptr_t)bump + sizeof(Block));

    if (bump_get_capacity(bump) > new_payload_size) 
        split_block(parent, (Block*)bump, new_payload_size);
}

/*
 * Reset a bump allocator
 * Resets the bump allocator's offset to the beginning
 */
void bump_reset(Bump *bump) {
    if (!bump) return;
    
    bump_set_offset(bump, sizeof(Bump));
}

/*
 * Free a bump allocator
 * Returns memory back to parent arena
 */
void bump_free(Bump *bump) {
    if (!bump) return;

    arena_free_block_full(bump_get_arena(bump), (Block *)(void *)bump);
}


#ifdef DEBUG

#ifdef USE_WPRINT
    #include <wchar.h>
    #define PRINTF wprintf
    #define T(str) L##str
#else
    #include <stdio.h>
    #define PRINTF printf
    #define T(str) str
#endif

/*
 * Helper function to print LLRB tree structure
 * Recursively prints the tree with indentation to show hierarchy
 */
void print_llrb_tree(Block *node, int depth) {
    if (node == NULL) return;
    
    // Print right subtree first (to display tree horizontally)
    print_llrb_tree(get_right_tree(node), depth + 1);
    
    // Print current node with indentation
    for (int i = 0; i < depth; i++) PRINTF(T("    "));
    PRINTF(T("Block: %p, Size: %lu %i\n"),
        node,
        get_size(node),
        get_color(node));
    
    // Print left subtree
    print_llrb_tree(get_left_tree(node), depth + 1);
}

/*
 * Print arena details
 * Outputs the current state of the arena and its blocks, including free blocks
 * Useful for debugging and understanding memory usage
 */
void print_arena(Arena *arena) {
    if (!arena) return;
    PRINTF(T("Arena: %p\n"), arena);
    PRINTF(T("Arena Full Size: %lu\n"), arena_get_capacity(arena) + sizeof(Arena));
    PRINTF(T("Arena Data Size: %lu\n"), arena_get_capacity(arena));
    PRINTF(T("Arena Alignment: %lu\n"), arena_get_alignment(arena));
    PRINTF(T("Data: %p\n"), (void *)((char *)arena + sizeof(Arena)));
    PRINTF(T("Tail: %p\n"), arena_get_tail(arena));
    PRINTF(T("Free Blocks: %p\n"), arena_get_free_blocks(arena));
    PRINTF(T("Free Size in Tail: %lu\n"), free_size_in_tail(arena));
    PRINTF(T("\n"));

    size_t occupied_data = 0;
    size_t occupied_meta = 0;
    size_t len = 0;

    Block *block = arena_get_first_block(arena);
    while (block != NULL) {
        occupied_data += get_size(block);
        occupied_meta += sizeof(Block);
        len++;
        PRINTF(T("  Block: %p\n"), block);
        PRINTF(T("  Block Full Size: %lu\n"), get_size(block) + sizeof(Block));
        PRINTF(T("  Block Data Size: %lu\n"), get_size(block));
        PRINTF(T("  Is Free: %d\n"), get_is_free(block));
        PRINTF(T("  Data Pointer: %p\n"), block_data(block));
        if (!get_is_free(block)) {
            PRINTF(T("  Magic: 0x%lx\n"), get_magic(block));
            PRINTF(T("  Arena: %p\n"), get_arena(block));
        }
        else {
            PRINTF(T("  Left Free: %p\n"), get_left_tree(block));
            PRINTF(T("  Right Free: %p\n"), get_right_tree(block));
        }
        PRINTF(T("  Color: %s\n"), get_color(block) ? "BLACK": "RED");
        PRINTF(T("  Next: %p\n"), next_block(arena, block));
        // PRINTF(T("  Next: %p\n"), next_block_unsafe(block));
        PRINTF(T("  Prev: %p\n"), get_prev(block));
        PRINTF(T("\n"));
        block = next_block(arena, block);
    }

    PRINTF(T("Arena Free Blocks\n"));

    Block *free_block = arena_get_free_blocks(arena);
    if (free_block == NULL) PRINTF(T("  None\n"));
    else {
        print_llrb_tree(free_block, 0);
    }
    PRINTF(T("\n"));

    PRINTF(T("Arena occupied data size: %lu\n"), occupied_data);
    PRINTF(T("Arena occupied meta size: %lu + %lu\n"), occupied_meta, sizeof(Arena));
    PRINTF(T("Arena occupied full size: %lu + %lu\n"), occupied_data + occupied_meta, sizeof(Arena));
    PRINTF(T("Arena block count: %lu\n"), len);
}

/*
 * Print a fancy visualization of the arena memory
 * Displays a bar chart of the arena's memory usage, including free blocks, occupied data, and metadata
 * Uses ANSI escape codes to colorize the visualization
 */
void print_fancy(Arena *arena, size_t bar_size) {
    if (!arena) return;
    
    size_t total_size = arena_get_capacity(arena);

    PRINTF(T("\nArena Memory Visualization [%zu bytes]\n"), total_size + sizeof(Arena));
    PRINTF(T("┌"));
    for (int i = 0; i < (int)bar_size; i++) PRINTF(T("─"));
    PRINTF(T("┐\n│"));

    // Size of one segment of visualization in bytes
    double segment_size = (double)(total_size / bar_size);
    
    // Iterate through each segment of visualization
    for (int i = 0; i < (int)bar_size; i++) {
        // Calculate the start and end positions of the segment in memory
        size_t segment_start = (size_t)(i * segment_size);
        size_t segment_end = (size_t)((i + 1) * segment_size);
        
        // Determine which data type prevails in this segment
        char segment_type = ' '; // Empty by default
        size_t max_overlap = 0;
        
        // Check arena metadata
        size_t arena_meta_end = sizeof(Arena);
        if (segment_start < arena_meta_end) {
            size_t overlap = segment_start < arena_meta_end ? 
                (arena_meta_end > segment_end ? segment_end - segment_start : arena_meta_end - segment_start) : 0;
            if (overlap > max_overlap) {
                max_overlap = overlap;
                segment_type = '@'; // Arena metadata
            }
        }
        
        // Check each block
        size_t current_pos = 0;
        Block *current = (Block *)((char *)arena + sizeof(Arena));
        
        while (current) {
            // Position of block metadata
            size_t block_meta_start = current_pos;
            size_t block_meta_end = block_meta_start + sizeof(Block);
            
            // Check intersection with block metadata
            if (segment_start < block_meta_end && segment_end > block_meta_start) {
                size_t overlap = (segment_end < block_meta_end ? segment_end : block_meta_end) - 
                             (segment_start > block_meta_start ? segment_start : block_meta_start);
                if (overlap > max_overlap) {
                    max_overlap = overlap;
                    segment_type = '@'; // Block metadata
                }
            }
            
            // Position of block data
            size_t block_data_start = block_meta_end;
            size_t block_data_end = block_data_start + get_size(current);
            
            // Check intersection with block data
            if (segment_start < block_data_end && segment_end > block_data_start) {
                // Calculate end point of overlap
                size_t overlap_end = segment_end;
                if (segment_end > block_data_end) {
                    overlap_end = block_data_end;
                }

                // Calculate start point of overlap
                size_t overlap_start = segment_start;
                if (segment_start < block_data_start) {
                    overlap_start = block_data_start;
                }

                size_t overlap = overlap_end - overlap_start;
                if (overlap > max_overlap) {
                    max_overlap = overlap;
                    segment_type = get_is_free(current) ? ' ' : '#'; // Free or occupied block
                }
            }
            
            current_pos = block_data_end;
            current = next_block(arena, current);
        }

        // Check tail free memory
        if (free_size_in_tail(arena) > 0) {
            size_t tail_start = total_size - free_size_in_tail(arena);
            if (segment_start < total_size && segment_end > tail_start) {
                size_t overlap = (segment_end < total_size ? segment_end : total_size) - 
                               (segment_start > tail_start ? segment_start : tail_start);
                if (overlap > max_overlap) {
                    max_overlap = overlap;
                    segment_type = '-'; // Free tail
                }
            }
        }
        
        // Display the corresponding symbol with color
        if (segment_type == '@') {
            PRINTF(T("\033[43m@\033[0m")); // Yellow for metadata
        } else if (segment_type == '#') {
            PRINTF(T("\033[41m#\033[0m")); // Red for occupied blocks
        } else if (segment_type == ' ') {
            PRINTF(T("\033[42m=\033[0m")); // Green for free blocks
        } else if (segment_type == '-') {
            PRINTF(T("\033[40m.\033[0m")); // Black for empty space
        }
    }

    PRINTF(T("│\n└"));
    for (int i = 0; i < (int)bar_size; i++) PRINTF(T("─"));
    PRINTF(T("┘\n"));

    PRINTF(T("Legend: "));
    PRINTF(T("\033[43m @ \033[0m - Used Meta blocks, "));
    PRINTF(T("\033[41m # \033[0m - Used Data blocks, "));
    PRINTF(T("\033[42m   \033[0m - Free blocks, "));
    PRINTF(T("\033[40m   \033[0m - Empty space\n\n"));
}
#endif // DEBUG

#endif // ARENA_IMPLEMENTATION

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ARENA_ALLOCATOR_H
