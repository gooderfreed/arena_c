#ifndef ARENA_ALLOCATOR_H
#define ARENA_ALLOCATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>


#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L) || defined(__cplusplus)
    #include <assert.h>
    #define ARENA_STATIC_ASSERT(cond, msg) static_assert(cond, #msg)
#else
    #define ARENA_STATIC_ASSERT_HELPER(cond, line) typedef char static_assertion_at_line_##line[(cond) ? 1 : -1]
    #define ARENA_STATIC_ASSERT(cond, msg) ARENA_STATIC_ASSERT_HELPER(cond, __LINE__)
#endif

#ifdef _WIN32
#include <BaseTsd.h> // SSIZE_T
#define ssize_t SSIZE_T
#else
#include <sys/types.h>  // for ssize_t
#endif

#ifndef MIN_BUFFER_SIZE
    // Default minimum buffer size for the arena.
    #define MIN_BUFFER_SIZE 16
#endif

#define DEFAULT_ALIGNMENT 16 // Default memory alignment

ARENA_STATIC_ASSERT((DEFAULT_ALIGNMENT >= 4), Default_alignment_must_be_at_least_4);
ARENA_STATIC_ASSERT((DEFAULT_ALIGNMENT > 0) && ((DEFAULT_ALIGNMENT & (DEFAULT_ALIGNMENT - 1)) == 0), Default_alignment_must_be_power_of_two);

#define FLAG_IS_FREE ((uintptr_t)1)    // Flag to indicate if a block is free
#define FLAG_COLOR   ((uintptr_t)2)    // Flag to indicate the color of a block in LLRB tree
#define POINTER_MASK (~(uintptr_t)3)   // Mask to extract the pointer without flags

#define ARENA_IS_DYNAMIC ((uintptr_t)1) // Flag to indicate if the arena is dynamic

#define RED false
#define BLACK true

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
    size_t size;          // Size of the data block.
    Block *prev;          // Pointer to the previous block in the global list, also stores flags via pointer tagging.

    union {
        struct {
            Block *left_free;     // Left child in red-black tree
            Block *right_free;    // Right child in red-black tree
        };
        struct {
            Arena *arena;         // Pointer to the arena that allocated this block
            void  *magic;         // Magic number for validation random pointer
        };
    };
};

/*
 * Bump allocator structure
 * A simple allocator that allocates memory linearly from a pre-allocated block
 */
struct Bump {
    union {
        Block *block_representation; // Block representation for compatibility
        struct {
            size_t capacity;         // Total capacity of the bump allocator
            Block *prev;             // Pointer to the previous block in the global list, need for compatibility with block struct layout
            Arena *arena;            // Pointer to the arena that allocated this block
            size_t offset;           // Current offset for allocations within the bump allocator
        };
    };
};

ARENA_STATIC_ASSERT((sizeof(Bump) == sizeof(Block)), Size_mismatch_between_Bump_and_Block);

/*
 * Memory arena structure
 * Manages a pool of memory, block allocation, and block states
 */
struct Arena {
    size_t capacity;                // Total capacity of the arena

    Block *tail;                    // Pointer to the last block in the global list, also stores is_dynamic flag via pointer tagging
    Block *free_blocks;             // Pointer to the tree of free blocks

    size_t free_size_in_tail;       // Free space available in the tail block
};

#ifndef ARENA_NO_MALLOC
Arena *arena_new_dynamic(ssize_t size);
void arena_free(Arena *arena);
#endif // ARENA_NO_MALLOC

Arena *arena_new_static(void *memory, ssize_t size);
void arena_reset(Arena *arena);

void *arena_alloc(Arena *arena, ssize_t size);
void *arena_calloc(Arena *arena, ssize_t nmemb, ssize_t size);

void arena_free_block(void *data);

Arena *arena_new_nested(Arena *parent_arena, ssize_t size);
void  arena_free_nested(Arena *nested_arena);

Bump  *bump_new(Arena *parent_arena, ssize_t size);
void  *bump_alloc(Bump *bump, ssize_t size);
void  bump_reset(Bump *bump);
void  bump_free(Bump *bump);

#ifdef DEBUG
#include <stdio.h>
#include <math.h>
void print_arena(Arena *arena);
void print_fancy(Arena *arena, size_t bar_size);
void print_llrb_tree(Block *node, int depth);
#endif // DEBUG


#ifdef ARENA_IMPLEMENTATION
/*
 * Get pointer
 * Retrieves the actual pointer address without flags
 */
static inline Block *get_pointer(Block *ptr) {
    return (Block *)((uintptr_t)ptr & POINTER_MASK);
}

/*
 * Get is free
 * Retrieves the is_free flag of the block pointer
 */
static inline bool get_is_free(Block *ptr) {
    return ((uintptr_t)ptr->prev & FLAG_IS_FREE);
}

/*
 * Get color
 * Retrieves the color flag of the block pointer
 */
static inline bool get_color(Block *ptr) {
    return ((uintptr_t)ptr->prev & FLAG_COLOR);
}

/*
 * Get is arena dynamic
 * Retrieves the is_dynamic flag of the arena pointer
 */
static inline bool get_is_arena_dynamic(Arena *arena) {
    return ((uintptr_t)arena->tail & ARENA_IS_DYNAMIC);
}

/*
 * Set is free
 * Updates the is_free flag of the block pointer
 */
static inline void set_is_free(Block *ptr, bool is_free) {
    uintptr_t int_ptr = (uintptr_t)(ptr->prev);
    if (is_free) {
        int_ptr |= FLAG_IS_FREE;
    }
    else {
        int_ptr &= ~FLAG_IS_FREE;
    }
    ptr->prev = (Block *)int_ptr;
}

/*
 * Set color
 * Updates the color flag of the block pointer
 */
static inline void set_color(Block *ptr, bool color) {
    uintptr_t int_ptr = (uintptr_t)(ptr->prev);
    if (color) {
        int_ptr |= FLAG_COLOR;
    }
    else {
        int_ptr &= ~FLAG_COLOR;
    }
    ptr->prev = (Block *)int_ptr;
}

/*
 * Set is arena dynamic
 * Updates the is_dynamic flag of the arena pointer
 */
static inline void set_is_arena_dynamic(Arena *arena, bool is_dynamic) {
    uintptr_t int_ptr = (uintptr_t)(arena->tail);
    if (is_dynamic) {
        int_ptr |= ARENA_IS_DYNAMIC;
    }
    else {
        int_ptr &= ~ARENA_IS_DYNAMIC;
    }
    arena->tail = (Block *)int_ptr;
}

/*
 * Override address
 * Combines the address of the target with the flags of the source
 */
static inline Block *override_address(Block *source, Block *target) {
    return (Block *)(((uintptr_t)source & ~POINTER_MASK) + ((uintptr_t)target & POINTER_MASK));
}

/*
 * Override previous block pointer
 * Safely updates the previous block pointer while preserving flags
 */
static inline void override_prev(Block *block, Block *new) {
    block->prev = override_address(block->prev, new);
}

/*
 * Override arena tail pointer
 * Safely updates the arena's tail pointer while preserving flags
 */
static inline void override_arena_tail(Arena *arena, Block *new) {
    arena->tail = override_address(arena->tail, new);
}

/*
 * Align up
 * Rounds up the given size to the nearest multiple of alignment
 */
static inline size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

/*
 * Safe next block pointer
 * Checks if the next block exists and is not in the tail free space
 */
static inline bool has_next_block(Arena *arena, Block *block) {
    // Calculate the offset of the next block (as a number, not a pointer)
    size_t next_offset = (size_t)((char *)block - (char *)arena + sizeof(Arena)) + 
                         sizeof(Block) + block->size;
    
    // Verify that the next block offset is within valid limits
    // And not in the tail free space
    return (next_offset < arena->capacity) && 
           ((block != get_pointer(arena->tail)) || (arena->free_size_in_tail == 0));
}

/*
 * Safe next block pointer
 * Checks if the next block exists and is not in the tail free space
 */
static inline Block *next_block(Arena *arena, Block *block) {
    if (!has_next_block(arena, block)) {
        return NULL;
    }
    
    // Only if the check passed, calculate the pointer
    return (Block *)(void *)((char *)block + sizeof(Block) + block->size);
}

/*
 * Rotate left
 * Used to balance the LLRB tree
 */
Block *rotateLeft(Block *current_block) {
    Block *x = current_block->right_free;
    current_block->right_free = x->left_free;
    x->left_free = current_block;
    set_color(x, get_color(current_block));
    set_color(current_block, RED);
    return x;
}

/*
 * Rotate right
 * Used to balance the LLRB tree
 */
Block *rotateRight(Block *current_block) {
    Block *x = current_block->left_free;
    current_block->left_free = x->right_free;
    x->right_free = current_block;
    set_color(x, get_color(current_block));
    set_color(current_block, RED);
    return x;
}

/*
 * Flip colors
 * Used to balance the LLRB tree
 */
void flipColors(Block *current_block) {
    set_color(current_block, RED);
    set_color(current_block->left_free, BLACK);
    set_color(current_block->right_free, BLACK);
}

/*
 * Balance the LLRB tree
 */
Block *balance(Block *current_block) {
    if (current_block->right_free && get_color(current_block->right_free) == RED)
        current_block = rotateLeft(current_block);
    if (current_block->left_free && get_color(current_block->left_free) == RED && current_block->left_free->left_free && get_color(current_block->left_free->left_free) == RED)
        current_block = rotateRight(current_block);
    if (current_block->left_free && current_block->right_free && get_color(current_block->left_free) == RED && get_color(current_block->right_free) == RED)
        flipColors(current_block);
    return current_block;
}

/*
 * Insert a new block into the LLRB tree
 */
Block *insert(Block *tree, Block *new_block) {
    if (tree == NULL) return new_block;

    if (new_block->size < tree->size)
        tree->left_free = insert(tree->left_free, new_block);
    else if (new_block->size > tree->size)
        tree->right_free = insert(tree->right_free, new_block);
    else {
        // If sizes are equal, compare addresses
        if (new_block < tree)
            tree->left_free = insert(tree->left_free, new_block);
        else
            tree->right_free = insert(tree->right_free, new_block);
    }

    tree = balance(tree);
    return tree;
}

/*
 * Detach a block from the LLRB tree
 */
void detach(Block **tree, Block *target) {
    if (!tree || !target) return;

    Block *parent = NULL;
    Block *current = *tree;

    while (current && current != target) {
        parent = current;
        if (target->size < current->size)
            current = current->left_free;
        else if (target->size > current->size)
            current = current->right_free;
        else {
            // If sizes are equal, compare addresses
            if (target < current)
                current = current->left_free;
            else
                current = current->right_free;
        }
    }

    if (!current) return; // In case target is not found

    Block *replacement = NULL;

    if (!target->right_free) {
        replacement = target->left_free;
    }
    else if (!target->left_free) {
        replacement = target->right_free;
    }
    else {
        Block *min_parent = target;
        Block *min_node = target->right_free;

        while (min_node->left_free) {
            min_parent = min_node;
            min_node = min_node->left_free;
        }

        if (min_parent != target) {
            min_parent->left_free = min_node->right_free;
            min_node->right_free = target->right_free;
        }

        min_node->left_free = target->left_free;
        replacement = min_node;
    }

    if (parent) {
        if (parent->left_free == target)
            parent->left_free = replacement;
        else
            parent->right_free = replacement;
    } else {
        *tree = replacement;
    }

    target->left_free = NULL;
    target->right_free = NULL;
    set_color(target, RED);

    if (*tree) {
        *tree = balance(*tree);
    }
}

/*
 * Find the best fit block in the LLRB tree
 */
Block *bestFit(Block *root, size_t size) {
    Block *best = NULL;
    Block *current = root;
    
    while (current) {
        if (current->size >= size) {
            // Update best if we found a better fit or if it's the first fit
            if (!best || current->size < best->size || 
                (current->size == best->size && current < best)) {
                best = current;
            }
            current = current->left_free;
        } else {
            current = current->right_free;
        }
    }

    return best;
}


/*
 * Make the given block the tail of the arena
 */
void make_tail(Arena *arena, Block *block) {
    arena->free_size_in_tail += block->size + sizeof(Block);
    override_arena_tail(arena, block);
    block->size = 0;
}

/*
 * Merge two adjacent free blocks
 */
Block *merge_blocks(Arena *arena, Block *target, Block *source) {
    target->size += source->size + sizeof(Block);
    Block *block_after = next_block(arena, source);
    if (block_after) {
        override_prev(block_after, target);
    }
    return target;
}


/*
 * Create an empty block at the end of the given block
 * Sets up a new block with size 0 and adjusts pointers
 */
static inline Block *create_empty_block(Block *prev_block) {
    // prep data for new block
    Block *prev_tail = prev_block;
    void *new_chunk = (char *)block_data(prev_tail) + prev_tail->size;

    // create new block
    Block *block = (Block *)new_chunk;
    block->size = 0;
    block->prev = NULL;
    set_color(block, RED);
    set_is_free(block, true);
    block->left_free = NULL;
    block->right_free = NULL;

    return block;
}


/*
 * Allocate memory from the tail of the arena
 * Updates the tail block and creates a new block if there is enough space
 */
static void *alloc_in_tail(Arena *arena, size_t size) {
    // get a tail block
    Block *block = get_pointer(arena->tail);
    // update block
    block->size = size;
    set_is_free(block, false);
    arena->free_size_in_tail -= size;
    block->arena = arena;
    block->magic = (void*)0xDEADBEEF;

    // create new block
    if (arena->free_size_in_tail >= sizeof(Block) + MIN_BUFFER_SIZE) {
        Block *new_block = create_empty_block(block);
        override_prev(new_block, block);
        // update arena
        override_arena_tail(arena, new_block);
        arena->free_size_in_tail -= sizeof(Block);
    }
    else {
        block->size += arena->free_size_in_tail;
        arena->free_size_in_tail = 0;
    }

    // return allocated data pointer
    return block_data(block);
}

/*
 * Allocate memory from the free blocks
 * Updates the free blocks tree and creates a new block if there is enough space
 */
static void *alloc_in_free_blocks(Arena *arena, size_t size) {
    Block *best = bestFit(arena->free_blocks, size);

    if (best) {
        detach(&arena->free_blocks, best);
        set_is_free(best, false);
        best->arena = arena;
        best->magic = (void*)0xDEADBEEF;
        if (best->size >= size + sizeof(Block) + MIN_BUFFER_SIZE) {
            Block *block_after = next_block(arena, best);
            size_t new_block_size = best->size - size - sizeof(Block);
            best->size = size;
            set_is_free(best, false);

            Block *new_block = create_empty_block(best);
            new_block->size = new_block_size;
            set_is_free(new_block, true);

            if (block_after) {
                override_prev(block_after, new_block);
            }
            override_prev(new_block, best);
            arena->free_blocks = insert(arena->free_blocks, new_block);
        }
        
        return block_data(best);
    }

    return NULL;
}

/*
 * Allocate memory in the arena
 * Tries to allocate memory in the tail or from free blocks
 * Returns NULL if there is not enough space
 */
void *arena_alloc(Arena *arena, ssize_t size) {
    if (size <= 0 || arena == NULL || (size_t)size > arena->capacity) return NULL;
    // check if there is enough space in the free blocks
    void *result = alloc_in_free_blocks(arena, align_up((size_t)size, DEFAULT_ALIGNMENT));
    if (result) return result;

    // check if arena has enough space in the end for aligned size
    if (arena->free_size_in_tail >= align_up((size_t)size, DEFAULT_ALIGNMENT)) {
        return alloc_in_tail(arena, align_up((size_t)size, DEFAULT_ALIGNMENT));
    }

    // check if arena has enough space in the literal end of memory (alignment for next allocation is not required)
    if (arena->free_size_in_tail >= (size_t)size) {
        return alloc_in_tail(arena, (size_t)size);
    }

    return NULL;
}

/*
 * Allocate zero-initialized memory in the arena
 * Returns NULL if there is not enough space or overflow is detected
 */
void *arena_calloc(Arena *arena, ssize_t nmemb, ssize_t size) {
    if (nmemb > 0 && (SIZE_MAX / nmemb) < (size_t)size) {
        return NULL; // Overflow detected
    }
    ssize_t total_size = nmemb * size;
    void *ptr = arena_alloc(arena, total_size);
    if (ptr) {
        memset(ptr, 0, total_size); // Zero-initialize the allocated memory
    }
    return ptr;
}

/*
 * Free a block of memory in the arena
 * Marks the block as free, merges it with adjacent free blocks if possible,
 * and updates the free block list
 */
static void arena_free_block_full(Arena *arena, void *data) {
    Block *block = (Block *)((void *)((char *)data - sizeof(Block)));
    set_is_free(block, true);
    block->left_free = NULL;
    block->right_free = NULL;
    set_color(block, RED);

    Block *prev = get_pointer(block->prev);
    Block *next = next_block(arena, block);

    Block* result = NULL;

    if (!next) {
        arena->free_size_in_tail += block->size;
        override_arena_tail(arena, block);
        block->size = 0;
    }
    else if (get_is_free(next)) {
        if (next == get_pointer(arena->tail)) {
            make_tail(arena, block);
        }
        else {
            detach(&arena->free_blocks, next);
            merge_blocks(arena, block, next);
            result = block;
        }
    }
    else {
        result = block;
    }

    if (prev && get_is_free(prev)) {
        detach(&arena->free_blocks, prev);
        if (block == get_pointer(arena->tail)) {
            make_tail(arena, prev);
        }
        else {
            merge_blocks(arena, prev, block);
            result = prev;
        }
    }

    if (result) {
        arena->free_blocks = insert(arena->free_blocks, result);
    }
}

/*
 * Free a block of memory in the arena
 * Marks the block as free, merges it with adjacent free blocks if possible,
 * and updates the free block list
 */
void arena_free_block(void *data) {
    if (!data) return;
    
    Block *block = (Block *)((void *)((char *)data - sizeof(Block)));
    
    if (block->magic != (void*)0xDEADBEEF) {
        return;
    }

    Arena *arena = block->arena;
    if (!arena ||(char *)data < (char *)arena + sizeof(Arena) || (char *)data > (char *)arena + sizeof(Arena) + arena->capacity) return;

    arena_free_block_full(arena, data);
}

/*
 * Create a static arena
 * Initializes an arena using preallocated memory and sets up the first block
 * Returns NULL if the provided size is too small, memory is NULL or size is negative
 */
Arena *arena_new_static(void *memory, ssize_t size) {
    if (!memory || size <= 0 || (size_t)size < sizeof(Arena) + sizeof(Block) + MIN_BUFFER_SIZE) return NULL;

    Arena *arena = (Arena *)memory;
    arena->capacity = (size_t)size - sizeof(Arena);
    arena->free_blocks = NULL;
    
    Block *block = (Block *)((char *)memory + sizeof(Arena));
    block->size = 0;
    block->prev = NULL;
    set_is_free(block, true);
    set_is_free(block, RED);
    block->left_free = NULL;
    block->right_free = NULL;

    override_arena_tail(arena, block);
    arena->free_size_in_tail = arena->capacity - sizeof(Block);

    set_is_arena_dynamic(arena, false);

    return arena;
}

#ifndef ARENA_NO_MALLOC
/*
 * Create a dynamic arena
 * Allocates memory for the arena and initializes it as a dynamic arena
 * Returns NULL if the requested size is too small or size is negative
 */
Arena *arena_new_dynamic(ssize_t size) {
    if (size <= 0 || (size_t)size < sizeof(Arena) + sizeof(Block) + MIN_BUFFER_SIZE) return NULL;
    void *data = malloc((size_t)size);
    if (!data) return NULL;
    Arena *arena = arena_new_static(data, size);

    set_is_arena_dynamic(arena, true);

    return arena;
}

/*
* Free a dynamic arena
* Releases memory for dynamically allocated arenas
*/
void arena_free(Arena *arena) {
    if (!arena) return;
    if (get_is_arena_dynamic(arena)) {
        free(arena);
    }
}
#endif // ARENA_NO_MALLOC

/*
 * Reset the arena
 * Clears the arena's blocks and resets it to the initial state without freeing memory
 */
void arena_reset(Arena *arena) {
    if (!arena) return;
    Block *block = (Block *)((char *)arena + sizeof(Arena));
    block->size = 0;
    block->prev = NULL;
    set_is_free(block, true);

    override_arena_tail(arena, block);
    arena->free_blocks = NULL;
    arena->free_size_in_tail = arena->capacity - sizeof(Block);
}

/*
 * Create a nested arena
 * Allocates memory for a nested arena from a parent arena and initializes it
 * Returns NULL if the parent arena is NULL, requested size is too small, or allocation fails
 */
Arena *arena_new_nested(Arena *parent_arena, ssize_t size) {
    if (!parent_arena) return NULL;
    if (size <= 0 || (size_t)size < sizeof(Arena) + sizeof(Block) + MIN_BUFFER_SIZE) return NULL;  // Check for minimal reasonable size
    void *data = arena_alloc(parent_arena, size);  // Allocate memory from the parent arena
    if (!data) return NULL;
    Arena *arena = arena_new_static(data, size);  // Initialize the nested arena in the allocated memory
    if (!arena) {
        arena_free_block(data);   // Free the allocated memory if arena initialization fails (should not happen because size is already checked)
        return NULL;
    }
    return arena;
}

/*
 * Free a nested arena
 * Releases memory for a nested arena back to its parent arena
 */
void arena_free_nested(Arena *nested_arena) {
    if (!nested_arena) return;
    arena_free_block((void *)nested_arena);
}

/*
 * Create a bump allocator
 * Initializes a bump allocator within a parent arena
 * Returns NULL if the parent arena is NULL, requested size is too small, or allocation fails
 */
Bump *bump_new(Arena *parent_arena, ssize_t size) {
    if (!parent_arena) return NULL;
    if (size <= 0 || (size_t)size < sizeof(Bump) + MIN_BUFFER_SIZE) return NULL;  // Check for minimal reasonable size
    void *data = arena_alloc(parent_arena, size);  // Allocate memory from the parent arena
    if (!data) return NULL;
    Bump *bump = (Bump *)((char *)data - sizeof(Block));  // just cast allocated memory to Bump

    bump->capacity = (size_t)(size) + sizeof(Block);
    bump->arena = parent_arena;
    bump->offset = sizeof(Bump);

    return bump;
}

/*
 * Allocate memory from a bump allocator
 * Returns a pointer to the allocated memory or NULL if allocation fails
 * May return NOT aligned pointer
 */
void *bump_alloc(Bump *bump, ssize_t size) {
    if (!bump) return NULL;
    if (size <= 0 || (size_t)size > bump->capacity - bump->offset) return NULL;
    void *memory = (char *)bump + bump->offset;
    bump->offset += size;

    return memory;
}

/*
 * Allocate aligned memory from a bump allocator
 * Returns a pointer to the allocated memory or NULL if allocation fails
 */
void *bump_alloc_aligned(Bump *bump, ssize_t size, size_t alignment) {
    if (!bump || size <= 0 || alignment == 0 || (alignment & (alignment - 1)) != 0) return NULL;

    uintptr_t current_ptr = (uintptr_t)bump + bump->offset;
    uintptr_t aligned_ptr = align_up(current_ptr, alignment);
    size_t padding = aligned_ptr - current_ptr;

    if ((size_t)size > SIZE_MAX - padding) return NULL;

    ssize_t total_size = padding + size;

    if ((size_t)total_size >= bump->capacity - bump->offset) return NULL;

    bump->offset += total_size;

    return (void *)aligned_ptr;
}

/*
 * Reset a bump allocator
 * Resets the bump allocator's offset to the beginning
 */
void bump_reset(Bump *bump) {
    if (!bump) return;
    bump->offset = sizeof(Bump);
}

/*
 * Free a bump allocator
 * Returns memory back to parent arena
 */
void bump_free(Bump *bump) {
    if (!bump) return;
    arena_free_block((char *)bump + sizeof(Bump));
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
    print_llrb_tree(node->right_free, depth + 1);
    
    // Print current node with indentation
    for (int i = 0; i < depth; i++) PRINTF(T("    "));
    PRINTF(T("Block: %p, Size: %lu %i\n"),
        node,
        node->size,
        get_color(node));
    
    // Print left subtree
    print_llrb_tree(node->left_free, depth + 1);
}

/*
 * Print arena details
 * Outputs the current state of the arena and its blocks, including free blocks
 * Useful for debugging and understanding memory usage
 */
void print_arena(Arena *arena) {
    PRINTF(T("Arena: %p\n"), arena);
    PRINTF(T("Arena Full Size: %lu\n"), arena->capacity + sizeof(Arena));
    PRINTF(T("Arena Data Size: %lu\n"), arena->capacity);
    PRINTF(T("Data: %p\n"), (void *)((char *)arena + sizeof(Arena)));
    PRINTF(T("Tail: %p\n"), get_pointer(arena->tail));
    PRINTF(T("Free Blocks: %p\n"), arena->free_blocks);
    PRINTF(T("Free Size in Tail: %lu\n"), arena->free_size_in_tail);
    PRINTF(T("\n"));

    size_t occupied_data = 0;
    size_t occupied_meta = 0;
    size_t len = 0;

    occupied_meta = sizeof(Arena);

    Block *block = (Block *)((char *)arena + sizeof(Arena));
    while (block != NULL) {
        occupied_data += block->size;
        occupied_meta += sizeof(Block);
        len++;
        PRINTF(T("  Block: %p\n"), block);
        PRINTF(T("  Block Full Size: %lu\n"), block->size + sizeof(Block));
        PRINTF(T("  Block Data Size: %lu\n"), block->size);
        PRINTF(T("  Is Free: %d\n"), get_is_free(block));
        PRINTF(T("  Data Pointer: %p\n"), block_data(block));
        PRINTF(T("  Arena: %p\n"), block->arena);
        PRINTF(T("  Next: %p\n"), next_block(arena, block));
        PRINTF(T("  Prev: %p\n"), get_pointer(block->prev));
        PRINTF(T("  Color: %s\n"), get_color(block) ? "RED" : "BLACK");
        PRINTF(T("  Left Free: %p\n"), block->left_free);
        PRINTF(T("  Right Free: %p\n"), block->right_free);
        PRINTF(T("\n"));
        block = next_block(arena, block);
    }

    PRINTF(T("Arena Free Blocks\n"));

    Block *free_block = arena->free_blocks;
    if (free_block == NULL) PRINTF(T("  None\n"));
    else {
        print_llrb_tree(free_block, 0);
    }
    PRINTF(T("\n"));

    PRINTF(T("Arena occupied data size: %lu\n"), occupied_data);
    PRINTF(T("Arena occupied meta size: %lu\n"), occupied_meta);
    PRINTF(T("Arena occupied full size: %lu\n"), occupied_data + occupied_meta);
    PRINTF(T("Arena block count: %lu\n"), len);
}

/*
 * Print a fancy visualization of the arena memory
 * Displays a bar chart of the arena's memory usage, including free blocks, occupied data, and metadata
 * Uses ANSI escape codes to colorize the visualization
 */
void print_fancy(Arena *arena, size_t bar_size) {
    if (!arena) return;
    
    size_t total_size = arena->capacity;

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
            size_t block_data_end = block_data_start + current->size;
            
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
        if (arena->free_size_in_tail > 0) {
            size_t tail_start = total_size - arena->free_size_in_tail;
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
            PRINTF(T("\033[42m \033[0m")); // Green for free blocks
        } else if (segment_type == '-') {
            PRINTF(T("\033[40m \033[0m")); // Black for empty space
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
