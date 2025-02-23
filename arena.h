#ifndef ARENA_ALLOCATOR_H
#define ARENA_ALLOCATOR_H

#include <stdbool.h>
#include <stdlib.h>

#ifndef MIN_BUFFER_SIZE
    // Default minimum buffer size for the arena.
    #define MIN_BUFFER_SIZE 16
#endif

// Structure type declarations for memory management.
typedef struct Block Block;
typedef struct Arena Arena;

/*
 * Memory block structure.
 * Represents a chunk of memory and metadata for its management within the arena.
 */
struct Block {
    size_t size;          // Size of the data block.
    void *data;           // Pointer to the start of user data in this block.
    Block *next;          // Pointer to the next block in the global list.
    Block *prev;          // Pointer to the previous block in the global list.

    bool is_free;         // Flag indicating whether the block is free.
    Block *next_free;     // Pointer to the next free block in the free list.
    Block *prev_free;     // Pointer to the previous free block in the free list.
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
    Block *free_blocks;                  // Pointer to the list of free blocks.

    size_t free_size_in_tail;            // Free space available in the tail block.
    size_t max_free_block_size;          // Size of the largest free block.
    size_t max_free_block_size_count;    // Number of blocks matching max_free_block_size.
};


Arena *arena_new_dynamic(size_t size);
Arena *arena_new_static(void *memory, size_t size);
void arena_reset(Arena *arena);
void *arena_alloc(Arena *arena, size_t size);
void arena_free_block(Arena *arena, void *data);
void arena_free(Arena *arena);

#ifdef DEBUG
#include <stdio.h>
#include <math.h>
void print_arena(Arena *arena);
void print_fancy(Arena *arena);
#endif // DEBUG

#ifdef ARENA_IMPLEMENTATION
static inline void update_next_of_prev_block(Arena *arena, Block *block, Block *new_val) {
    if (block->prev_free) {
        block->prev_free->next_free = new_val;
    } else {
        arena->free_blocks = new_val;
    }
}

static inline void update_prev_of_next_block(Block *block, Block *new_val) {
    if (block->next_free) {
        block->next_free->prev_free = new_val;
    }
}

static inline void update_max_free_block_on_free(Arena *arena, size_t block_size) {
    if (block_size > arena->max_free_block_size) {
        arena->max_free_block_size = block_size;
        arena->max_free_block_size_count = 1;
    } else if (block_size == arena->max_free_block_size) {
        arena->max_free_block_size_count++;
    }
}

static inline Block *create_empty_block(Arena *arena, Block *prev_block) {
    // prep data for new block
    Block *prev_tail = prev_block;
    void *new_chunk = prev_tail->data + prev_tail->size;

    // create new block
    Block *block = (Block *)new_chunk;
    block->size = 0;
    block->data = new_chunk + sizeof(Block);
    block->is_free = true;
    block->next = NULL;
    block->prev = NULL;

    return block;
}

static inline size_t find_second_largest_size(Arena *arena) {
    Block *block = arena->free_blocks;
    if (!block) return 0;

    size_t second_largest = 0;
    while (block) {
        if (block->size > second_largest && block->size != arena->max_free_block_size) {
            second_largest = block->size;
        }
        block = block->next_free;
    }
    return second_largest;
}

static inline void update_max_free_block_on_detach(Arena *arena, size_t block_size) {
    if (block_size == arena->max_free_block_size) {
        arena->max_free_block_size_count--;
        if (arena->max_free_block_size_count == 0) {
            arena->max_free_block_size = find_second_largest_size(arena);
            if (arena->max_free_block_size != 0) arena->max_free_block_size_count = 1;
        }
    }
}

static void wipe_free_block(Arena *arena, Block *free_block, Block *tail) {
    if (tail->next == NULL && tail->size == 0) {
        free_block->next = NULL;

        arena->free_size_in_tail += free_block->size + sizeof(Block);
        arena->tail = free_block;

        update_max_free_block_on_detach(arena, free_block->size);
        update_next_of_prev_block(arena, free_block, free_block->next_free);
        update_prev_of_next_block(free_block, free_block->prev_free);

        free_block->size = 0;
    }
}

static void merge_blocks(Arena *arena, Block *block1, Block *block2) {
    // wipe free block if it's the last block
    if (block2->next == NULL && block2->size == 0) {
        wipe_free_block(arena, block1, block2);
        return;
    }

    // merge blocks
    block1->size += block2->size + sizeof(Block);
    Block *block_after = block2->next;
    block_after->prev = block1;
    block1->next = block_after;

    update_next_of_prev_block(arena, block2, block2->next_free);
    update_max_free_block_on_free(arena, block1->size);
}

static void split_block(Arena *arena, Block *block, size_t size) {
    size_t new_block_size = block->size - size - sizeof(Block);
    size_t block_size = block->size;
    block->size = size;
    block->is_free = false;

    Block *new_block = create_empty_block(arena, block);
    new_block->size = new_block_size;
    new_block->is_free = true;

    if (block->next) {
        new_block->next = block->next;
        block->next->prev = new_block;
    }
    else {
        new_block->next = NULL;
    }
    block->next = new_block;
    new_block->prev = block;

    update_next_of_prev_block(arena, block, new_block);
    update_prev_of_next_block(block, new_block);

    new_block->next_free = block->next_free;
    new_block->prev_free = block->prev_free;

    update_max_free_block_on_detach(arena, block_size);
}

static void *alloc_in_tail(Arena *arena, size_t size) {
    // get a tail block
    Block *block = arena->tail;
    // update block
    block->size = size;
    block->is_free = false;
    arena->free_size_in_tail -= size;

    // create new block
    if (arena->free_size_in_tail >= sizeof(Block)) {
        Block *new_block = create_empty_block(arena, arena->tail);
        block->next = new_block;
        new_block->prev = block;
        // update arena
        arena->tail = new_block;
        arena->free_size_in_tail -= sizeof(Block);
    }
    else {
        block->size += arena->free_size_in_tail;
        arena->free_size_in_tail = 0;
    }

    // return allocated data pointer
    return block->data;
}

static void *alloc_in_free_block(Arena *arena, size_t size) {
    Block *block = arena->free_blocks;

    // find block with enough space
    while (block->size < size) {
        block = block->next_free;
    }

    // split block if it's too big
    if (block->size > size + sizeof(Block) + MIN_BUFFER_SIZE) {
        split_block(arena, block, size);
    }
    else {
        block->is_free = false;

        update_next_of_prev_block(arena, block, block->next_free);
        update_prev_of_next_block(block, block->prev_free);
        update_max_free_block_on_detach(arena, block->size);
    }

    return block->data;
}

void *arena_alloc(Arena *arena, size_t size) {
    if (size == 0) return NULL;

    // check if area has enough space in the end
    if (arena->free_size_in_tail >= size) {
        return alloc_in_tail(arena, size);
    }

    // check if there are free blocks with enough space
    if (arena->max_free_block_size >= size) {
        return alloc_in_free_block(arena, size);
    }
    return NULL;
}

void arena_free_block(Arena *arena, void *data) {
    Block *block = (Block *)(data - sizeof(Block));
    block->is_free = true;

    Block *prev = block->prev;
    Block *next = block->next;

    update_max_free_block_on_free(arena, block->size);

    if (!next) {
        block->is_free = true;
        arena->free_size_in_tail += block->size;
        block->size = 0;
    }
    else if (next->is_free) {
        merge_blocks(arena, block, next);
    }

    if (prev && prev->is_free) {
        merge_blocks(arena, prev, block);
        block = prev;
    }

    if (block->size != 0) {
        if (!arena->free_blocks) {
            arena->free_blocks = block;
        } else {
            Block *prev_free = arena->free_blocks;
            block->next_free = prev_free;
            prev_free->prev_free = block;
            arena->free_blocks = block;
        }
    }
}

Arena *arena_new_static(void *memory, size_t size) {
    if (size < sizeof(Arena) + sizeof(Block) + MIN_BUFFER_SIZE) return NULL;

    Arena *arena = (Arena *)memory;
    arena->capacity = size - sizeof(Arena);
    arena->data = memory + sizeof(Arena);
    arena->max_free_block_size = 0;
    arena->max_free_block_size_count = 0;
    
    Block *block = (Block *)arena->data;
    block->size = 0;
    block->data = arena->data + sizeof(Block);
    block->is_free = true;
    block->next = NULL;
    block->prev = NULL;

    arena->tail = block;
    arena->free_size_in_tail = arena->capacity - sizeof(Block);

    arena->is_dynamic = false;

    return arena;
}

Arena *arena_new_dynamic(size_t size) {
    if (size < sizeof(Arena) + sizeof(Block) + MIN_BUFFER_SIZE) return NULL;
    void *data = malloc(size);
    Arena *arena = arena_new_static(data, size);

    arena->is_dynamic = true;

    return arena;
}

void arena_reset(Arena *arena) {
    Block *block = (Block *)arena->data;
    block->size = 0;
    block->is_free = true;
    block->next = NULL;
    block->prev = NULL;
    block->next_free = NULL;
    block->prev_free = NULL;

    arena->tail = block;
    arena->free_blocks = NULL;
    arena->free_size_in_tail = arena->capacity - sizeof(Block);
}

void arena_free(Arena *arena) {
    if (arena->is_dynamic) {
        free(arena);
    }
}

#ifdef DEBUG
void print_arena(Arena *arena) {
    printf("Arena: %p\n", arena);
    printf("Arena Full Size: %lu\n", arena->capacity + sizeof(Arena));
    printf("Arena Data Size: %lu\n", arena->capacity);
    printf("Data: %p\n", arena->data);
    printf("Tail: %p\n", arena->tail);
    printf("Free Size in Tail: %lu\n", arena->free_size_in_tail);
    printf("Max Free Block Size: %lu\n", arena->max_free_block_size);
    printf("Max Free Block Size Count: %lu\n", arena->max_free_block_size_count);
    printf("\n");

    Block *block = (Block *)arena->data;
    while (block != NULL) {
        printf("  Block: %p\n", block);
        printf("  Block Full Size: %lu\n", block->size + sizeof(Block));
        printf("  Block Data Size: %lu\n", block->size);
        printf("  Is Free: %d\n", block->is_free);
        printf("  Data Pointer: %p\n", block->data);
        printf("  Next: %p\n", block->next);
        printf("  Prev: %p\n", block->prev);
        printf("\n");
        block = block->next;
    }

    printf("Arena Free Blocks\n");

    Block *free_block = arena->free_blocks;
    if (free_block == NULL) printf("  None\n");
    while (free_block != NULL) {
        printf("  Block: %p\n", free_block);
        printf("  Block Full Size: %lu\n", free_block->size + sizeof(Block));
        printf("  Block Data Size: %lu\n", free_block->size);
        printf("  Is Free: %d\n", free_block->is_free);
        printf("  Data Pointer: %p\n", free_block->data);
        printf("  Next: %p\n", free_block->next);
        printf("  Prev: %p\n", free_block->prev);
        printf("  Next Free: %p\n", free_block->next_free);
        printf("  Prev Free: %p\n", free_block->prev_free);
        printf("\n");
        free_block = free_block->next_free;
    }
    printf("\n");
}

#ifndef BAR_SIZE
    #define BAR_SIZE 102
#endif

void print_fancy(Arena *arena) {
    char bar_borders[BAR_SIZE + 1 + 2];
    char bar_data[BAR_SIZE + 1 + 2];

    for (int i = 0; i < BAR_SIZE; i++) {
        bar_borders[i] = '-';
        bar_data[i] = ' ';
    }

    bar_borders[0] = '+';
    bar_data[0] = '|';
    bar_borders[BAR_SIZE - 1] = '+';
    bar_data[BAR_SIZE - 1] = '|';

    bar_borders[BAR_SIZE] = '\0';
    bar_data[BAR_SIZE] = '\0';

    double scale_factor = (double)BAR_SIZE / (double)(arena->capacity + sizeof(Arena));

    double area_meta_size = floor(sizeof(Arena) * scale_factor);
    for (int i = 0; i < area_meta_size; i++) {
        bar_data[i + 1] = '@';
    }
    bar_borders[(int)area_meta_size] = '+';
    bar_data[(int)area_meta_size] = '|';

    Block *block = (Block *)arena->data;
    double block_meta_size = floor(sizeof(Block) * scale_factor);
    int counter = (int)area_meta_size;

    while (block != NULL) {
        double block_size = floor(block->size * scale_factor);
        for (int i = 0; i < block_meta_size; i++) {
            bar_data[++counter] = '@';
        }
        bar_data[counter] = '|';
        for (int i = 0; i < block_size; i++) {
            if (!block->is_free) bar_data[++counter] = '#';
            else bar_data[++counter] = ' ';
        }
        bar_borders[++counter] = '+';
        bar_data[counter] = '|';
        block = block->next;
    }

    printf("%s\n", bar_borders);
    printf("%s\n", bar_data);
    printf("%s\n", bar_borders);
}
#endif // DEBUG

#endif // ARENA_IMPLEMENTATION

#endif // ARENA_ALLOCATOR_H
