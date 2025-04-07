#ifndef ARENA_ALLOCATOR_H
#define ARENA_ALLOCATOR_H

#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>  // for ssize_t

#ifndef MIN_BUFFER_SIZE
    // Default minimum buffer size for the arena.
    #define MIN_BUFFER_SIZE 16
#endif

#define RED false
#define BLACK true

#define block_data(block) ((void *)((char *)(block) + sizeof(Block)))

// Structure type declarations for memory management.
typedef struct Block Block;
typedef struct Arena Arena;

/*
 * Union for block flags
 * Used to store the flags of a block in a single byte
 */
typedef union BlockFlags {
    struct {
        bool is_free     : 1;    // Flag indicating whether the block is free.
        bool color       : 1;    // Color for RB tree: 0 = RED, 1 = BLACK
        unsigned padding : 6;    // Padding to make the union size 8 bytes
    } bits;
    char raw;                    // Raw byte value, used for comparison as a magic number
} BlockFlags;


/*
 * Memory block structure.
 * Represents a chunk of memory and metadata for its management within the arena.
 */
struct Block {
    size_t size;          // Size of the data block.
    Block *prev;          // Pointer to the previous block in the global list.

    Arena *arena;         // Pointer to the arena that allocated this block.

    BlockFlags flags;
    
    Block *left_free;     // Left child in red-black tree
    Block *right_free;    // Right child in red-black tree
};

/*
 * Memory arena structure.
 * Manages a pool of memory, block allocation, and block states.
 */
struct Arena {
    size_t capacity;                     // Total capacity of the arena.
    void *data;                          // Pointer to the start of the memory managed by the arena.

    bool is_dynamic;                     // Flag indicating if the arena uses dynamic allocation.

    Block *tail;                         // Pointer to the last block in the global list.
    Block *free_blocks;                  // Pointer to the list of free blocks (становится корнем RB-дерева).

    size_t free_size_in_tail;            // Free space available in the tail block.
};


Arena *arena_new_dynamic(ssize_t size);
Arena *arena_new_static(void *memory, ssize_t size);
void arena_reset(Arena *arena);
void *arena_alloc(Arena *arena, size_t size);
void arena_free_block(void *data);
void arena_free(Arena *arena);

#ifdef DEBUG
#include <stdio.h>
#include <math.h>
void print_arena(Arena *arena);
void print_fancy(Arena *arena, size_t bar_size);
void print_llrb_tree(Block *node, int depth);
#endif // DEBUG


#ifdef ARENA_IMPLEMENTATION
/*
 * Safe next block pointer
 * Checks if the next block exists and is not in the tail free space
 */
static inline bool has_next_block(Arena *arena, Block *block) {
    // Calculate the offset of the next block (as a number, not a pointer)
    size_t next_offset = (size_t)((char *)block - (char *)arena->data) + 
                         sizeof(Block) + block->size;
    
    // Verify that the next block offset is within valid limits
    // And not in the tail free space
    return (next_offset < arena->capacity) && 
           ((block != arena->tail) || (arena->free_size_in_tail == 0));
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
    x->flags.bits.color = current_block->flags.bits.color;
    current_block->flags.bits.color = RED;
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
    x->flags.bits.color = current_block->flags.bits.color;
    current_block->flags.bits.color = RED;
    return x;
}

/*
 * Flip colors
 * Used to balance the LLRB tree
 */
void flipColors(Block *current_block) {
    current_block->flags.bits.color = RED;
    current_block->left_free->flags.bits.color = BLACK;
    current_block->right_free->flags.bits.color = BLACK;
}

/*
 * Balance the LLRB tree
 */
Block *balance(Block *current_block) {
    if (current_block->right_free && current_block->right_free->flags.bits.color == RED)
        current_block = rotateLeft(current_block);
    if (current_block->left_free && current_block->left_free->flags.bits.color == RED && current_block->left_free->left_free && current_block->left_free->left_free->flags.bits.color == RED)
        current_block = rotateRight(current_block);
    if (current_block->left_free && current_block->right_free && current_block->left_free->flags.bits.color == RED && current_block->right_free->flags.bits.color == RED)
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
    target->flags.bits.color = RED;

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
    arena->tail = block;
    block->size = 0;
}

/*
 * Merge two adjacent free blocks
 */
Block *merge_blocks(Arena *arena, Block *target, Block *source) {
    target->size += source->size + sizeof(Block);
    Block *block_after = next_block(arena, source);
    if (block_after) {
        block_after->prev = target;
    }
    return target;
}


/*
 * Create an empty block at the end of the given block
 * Sets up a new block with size 0 and adjusts pointers
 */
static inline Block *create_empty_block(Arena *arena, Block *prev_block) {
    // prep data for new block
    Block *prev_tail = prev_block;
    void *new_chunk = (char *)block_data(prev_tail) + prev_tail->size;

    // create new block
    Block *block = (Block *)new_chunk;
    block->size = 0;
    block->flags.bits.is_free = true;
    block->prev = NULL;
    block->flags.bits.color = RED;
    block->left_free = NULL;
    block->right_free = NULL;
    block->arena = arena;

    return block;
}


/*
 * Allocate memory from the tail of the arena
 * Updates the tail block and creates a new block if there is enough space
 */
static void *alloc_in_tail(Arena *arena, size_t size) {
    // get a tail block
    Block *block = arena->tail;
    // update block
    block->size = size;
    block->flags.bits.is_free = false;
    arena->free_size_in_tail -= size;

    // create new block
    if (arena->free_size_in_tail >= sizeof(Block)) {
        Block *new_block = create_empty_block(arena, arena->tail);
        new_block->prev = block;
        // update arena
        arena->tail = new_block;
        arena->free_size_in_tail -= sizeof(Block);
    }
    else {
        block->size += arena->free_size_in_tail;
        arena->free_size_in_tail = 0;
    }

    block->arena = arena;
    // return allocated data pointer
    return block_data(block);
}

/*
 * Allocate memory from the free blocks
 * Updates the free blocks list and creates a new block if there is enough space
 */
static void *alloc_in_free_blocks(Arena *arena, size_t size) {
    Block *best = bestFit(arena->free_blocks, size);

    if (best) {
        detach(&arena->free_blocks, best);
        best->flags.bits.is_free = false;
        if (best->size >= size + sizeof(Block) + MIN_BUFFER_SIZE) {
            Block *block_after = next_block(arena, best);
            size_t new_block_size = best->size - size - sizeof(Block);
            best->size = size;
            best->flags.bits.is_free = false;

            Block *new_block = create_empty_block(arena, best);
            new_block->size = new_block_size;
            new_block->flags.bits.is_free = true;

            if (block_after) {
                block_after->prev = new_block;
            }
            new_block->prev = best;
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
void *arena_alloc(Arena *arena, size_t size) {
    if (size == 0 || arena == NULL || size > arena->capacity) return NULL;
    // check if there is enough space in the free blocks
    void *result = alloc_in_free_blocks(arena, size);
    if (result) return result;

    // check if area has enough space in the end
    if (arena->free_size_in_tail >= size) {
        return alloc_in_tail(arena, size);
    }

    return NULL;
}

/*
 * Free a block of memory in the arena
 * Marks the block as free, merges it with adjacent free blocks if possible,
 * and updates the free block list
 */
static void arena_free_block_full(Arena *arena, void *data) {
    Block *block = (Block *)((void *)((char *)data - sizeof(Block)));
    block->flags.bits.is_free = true;
    block->left_free = NULL;
    block->right_free = NULL;
    block->flags.bits.color = RED;

    Block *prev = block->prev;
    Block *next = next_block(arena, block);

    Block* result = NULL;

    if (!next) {
        arena->free_size_in_tail += block->size;
        arena->tail = block;
        block->size = 0;
    }
    else if (next->flags.bits.is_free) {
        if (next == arena->tail) {
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

    if (prev && prev->flags.bits.is_free) {
        detach(&arena->free_blocks, prev);
        if (block == arena->tail) {
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
    
    char *flags_byte = (char *)&block->flags.raw;
    if (*flags_byte & ~0x3) {  // 0x3 = 0b00000011 - mask for two least significant bits
        return; 
    }

    Arena *arena = block->arena;
    if (!arena ||(char *)data < (char *)arena->data || (char *)data > (char *)arena->data + arena->capacity) return;
    
    arena_free_block_full(arena, data);
}

/*
 * Create a static arena
 * Initializes an arena using preallocated memory and sets up the first block
 * Returns NULL if the provided size is too small, memory is NULL or size is negative
 */
Arena *arena_new_static(void *memory, ssize_t size) {
    if (!memory || size < 0 || (size_t)size < sizeof(Arena) + sizeof(Block) + MIN_BUFFER_SIZE) return NULL;

    Arena *arena = (Arena *)memory;
    arena->capacity = (size_t)size - sizeof(Arena);
    arena->data = (char *)memory + sizeof(Arena);
    
    Block *block = (Block *)arena->data;
    block->size = 0;
    block->flags.bits.is_free = true;
    block->prev = NULL;

    arena->tail = block;
    arena->free_size_in_tail = arena->capacity - sizeof(Block);

    arena->is_dynamic = false;

    return arena;
}

/*
 * Create a dynamic arena
 * Allocates memory for the arena and initializes it as a dynamic arena
 * Returns NULL if the requested size is too small or size is negative
 */
Arena *arena_new_dynamic(ssize_t size) {
    if (size < 0 || (size_t)size < sizeof(Arena) + sizeof(Block) + MIN_BUFFER_SIZE) return NULL;
    void *data = malloc((size_t)size);
    if (!data) return NULL;
    Arena *arena = arena_new_static(data, size);

    arena->is_dynamic = true;

    return arena;
}

/*
 * Reset the arena
 * Clears the arena's blocks and resets it to the initial state without freeing memory
 */
void arena_reset(Arena *arena) {
    if (!arena) return;
    Block *block = (Block *)arena->data;
    block->size = 0;
    block->flags.bits.is_free = true;
    block->prev = NULL;

    arena->tail = block;
    arena->free_blocks = NULL;
    arena->free_size_in_tail = arena->capacity - sizeof(Block);
}

/*
 * Free a dynamic arena
 * Releases memory for dynamically allocated arenas
 */
void arena_free(Arena *arena) {
    if (!arena) return;
    if (arena->is_dynamic) {
        free(arena);
    }
}

#ifdef DEBUG
/*
 * Helper function to print LLRB tree structure
 * Recursively prints the tree with indentation to show hierarchy
 */
void print_llrb_tree(Block *node, int depth) {
    if (node == NULL) return;
    
    // Print right subtree first (to display tree horizontally)
    print_llrb_tree(node->right_free, depth + 1);
    
    // Print current node with indentation
    for (int i = 0; i < depth; i++) printf("    ");
    printf("Block: %p, Size: %lu %i\n",
        node, 
        node->size,
        node->flags.bits.color);
    
    // Print left subtree
    print_llrb_tree(node->left_free, depth + 1);
}

/*
 * Print arena details
 * Outputs the current state of the arena and its blocks, including free blocks
 * Useful for debugging and understanding memory usage
 */
void print_arena(Arena *arena) {
    printf("Arena: %p\n", arena);
    printf("Arena Full Size: %lu\n", arena->capacity + sizeof(Arena));
    printf("Arena Data Size: %lu\n", arena->capacity);
    printf("Data: %p\n", arena->data);
    printf("Tail: %p\n", arena->tail);
    printf("Free Blocks: %p\n", arena->free_blocks);
    printf("Free Size in Tail: %lu\n", arena->free_size_in_tail);
    printf("\n");

    size_t occupied_data = 0;
    size_t occupied_meta = 0;
    size_t len = 0;

    occupied_meta = sizeof(Arena);

    Block *block = (Block *)arena->data;
    while (block != NULL) {
        occupied_data += block->size;
        occupied_meta += sizeof(Block);
        len++;
        printf("  Block: %p\n", block);
        printf("  Block Full Size: %lu\n", block->size + sizeof(Block));
        printf("  Block Data Size: %lu\n", block->size);
        printf("  Is Free: %d\n", block->flags.bits.is_free);
        printf("  Data Pointer: %p\n", block_data(block));
        printf("  Arena: %p\n", block->arena);
        printf("  Next: %p\n", next_block(arena, block));
        printf("  Prev: %p\n", block->prev);
        printf("  Color: %s\n", block->flags.bits.color ? "RED" : "BLACK");
        printf("  Left Free: %p\n", block->left_free);
        printf("  Right Free: %p\n", block->right_free);
        printf("\n");
        block = next_block(arena, block);
    }

    printf("Arena Free Blocks\n");

    Block *free_block = arena->free_blocks;
    if (free_block == NULL) printf("  None\n");
    else {
        print_llrb_tree(free_block, 0);
    }
    printf("\n");

    printf("Arena occupied data size: %lu\n", occupied_data);
    printf("Arena occupied meta size: %lu\n", occupied_meta);
    printf("Arena occupied full size: %lu\n", occupied_data + occupied_meta);
    printf("Arena block count: %lu\n", len);
}

/*
 * Print a fancy visualization of the arena memory
 * Displays a bar chart of the arena's memory usage, including free blocks, occupied data, and metadata
 * Uses ANSI escape codes to colorize the visualization
 */
void print_fancy(Arena *arena, size_t bar_size) {
    if (!arena) return;
    
    size_t total_size = arena->capacity;

    printf("\nArena Memory Visualization [%zu bytes]\n", total_size + sizeof(Arena));
    printf("┌");
    for (int i = 0; i < (int)bar_size; i++) printf("─");
    printf("┐\n│");
    
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
        Block *current = (Block *)arena->data;
        
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
                size_t overlap = (segment_end < block_data_end ? segment_end : block_data_end) - 
                             (segment_start > block_data_start ? segment_start : block_data_start);
                if (overlap > max_overlap) {
                    max_overlap = overlap;
                    segment_type = current->flags.bits.is_free ? ' ' : '#'; // Free or occupied block
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
            printf("\033[43m@\033[0m"); // Yellow for metadata
        } else if (segment_type == '#') {
            printf("\033[41m#\033[0m"); // Red for occupied blocks
        } else if (segment_type == ' ') {
            printf("\033[42m \033[0m"); // Green for free blocks
        } else if (segment_type == '-') {
            printf("\033[40m \033[0m"); // Black for empty space
        }
    }
    
    printf("│\n└");
    for (int i = 0; i < (int)bar_size; i++) printf("─");
    printf("┘\n");

    printf("Legend: ");
    printf("\033[43m @ \033[0m - Used Meta blocks, ");
    printf("\033[41m # \033[0m - Used Data blocks, ");
    printf("\033[42m   \033[0m - Free blocks, ");
    printf("\033[40m   \033[0m - Empty space\n\n");
}
#endif // DEBUG

#endif // ARENA_IMPLEMENTATION

#endif // ARENA_ALLOCATOR_H
