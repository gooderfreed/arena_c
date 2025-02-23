# Header-Only Arena Allocator in C

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**A fast, efficient, and easy-to-use header-only arena memory allocator library written in pure C.**

## Overview

This library provides a header-only implementation of an arena memory allocator in C. Arena allocation is a memory management technique that allocates memory in large chunks (arenas) and then subdivides those chunks into smaller blocks for application use. This approach offers significant performance benefits and control over memory allocation and deallocation, especially in scenarios with frequent allocations and deallocations of objects of similar sizes.

**Key Benefits of Arena Allocation:**

*   **Performance:** Faster allocation and deallocation compared to general-purpose allocators like `malloc`/`free`, especially for allocating and freeing many small objects.
*   **Efficiency:** Reduced memory fragmentation, leading to better memory utilization.
*   **Control:**  Deterministic deallocation - freeing the entire arena at once is very fast and predictable.
*   **Simplicity:**  Easy to integrate into C projects as a header-only library.

**When to Use Arena Allocation:**

*   Game development:  For managing game objects, particles, and temporary data.
*   Real-time systems:  Where predictable allocation and deallocation times are crucial.
*   Parsing and data processing:  For managing temporary data structures during parsing or processing large datasets.
*   Any application where you need to allocate and deallocate many objects of similar sizes in a controlled and efficient manner.

## Features

*   **Header-Only Library:**  Easy integration - just include `arena_allocator.h` in your project.
*   **Static and Dynamic Arena Creation:**
    *   **Static Arena:**  Create arenas in pre-allocated memory regions for maximum control and predictability.
    *   **Dynamic Arena:**  Dynamically allocate arena memory using `malloc`.
*   **Fast Allocation and Deallocation:**  Optimized for speed, especially for frequent allocations and deallocations within the arena.
*   **Memory Efficiency:**  Reduces fragmentation and improves memory utilization.
*   **Arena Reset:**  Quickly reset the entire arena, freeing all allocated blocks at once.
*   **Debugging Tools:**  Includes `print_arena` and `print_fancy` functions (enabled via `DEBUG` macro) for detailed arena state inspection and visualization during development.
*   **Customizable Minimum Block Size:**  Adjust `MIN_BUFFER_SIZE` macro to fine-tune memory usage based on your application needs.
*   **Clear and Well-Commented Code:**  Easy to understand and modify.
*   **MIT License:**  Permissive open-source license.

## Getting Started

### 1. Include the Header File

```c
#include "arena_allocator.h"
```

Make sure to copy `arena_allocator.h` into your project's include directory or adjust the include path accordingly.

### 2. Implement Arena Allocator Functions (Once per Project)

In **one** of your `.c` files (e.g., `arena_impl.c`), define the `ARENA_IMPLEMENTATION` macro **before** including `arena_allocator.h`:

```c
#define ARENA_IMPLEMENTATION
#include "arena_allocator.h"
```

**Important:**  Define `ARENA_IMPLEMENTATION` in **only one** C file in your project. This will compile the implementation of the arena allocator functions.  In all other files where you use the arena allocator, just include `arena_allocator.h` without defining `ARENA_IMPLEMENTATION`.

### 3. Create an Arena

**Dynamic Arena (Memory allocated using `malloc`):**

```c
#include "arena_allocator.h"

int main() {
    size_t arena_size = 1024 * 1024; // 1MB arena
    Arena *arena = arena_new_dynamic(arena_size);
    if (!arena) {
        // Handle allocation error
        return 1;
    }

    // ... use the arena ...

    arena_free(arena); // Free the dynamically allocated arena memory
    return 0;
}
```

**Static Arena (Using pre-allocated memory):**

```c
#include "arena_allocator.h"

#define ARENA_SIZE (1024 * 1024) // 1MB arena
char arena_memory[ARENA_SIZE];

int main() {
    Arena *arena = arena_new_static(arena_memory, ARENA_SIZE);
    if (!arena) {
        // Handle initialization error (size too small)
        return 1;
    }

    // ... use the arena ...

    // No need to arena_free for static arena, memory is statically allocated
    return 0;
}
```

### 4. Allocate Memory from the Arena

```c
Arena *arena = arena_new_dynamic(1024 * 1024);
if (!arena) return 1;

// Allocate memory for an integer
int *my_int = (int *)arena_alloc(arena, sizeof(int));
if (my_int) {
    *my_int = 42;
}

// Allocate memory for a structure
typedef struct {
    int x;
    int y;
} Point;
Point *my_point = (Point *)arena_alloc(arena, sizeof(Point));
if (my_point) {
    my_point->x = 10;
    my_point->y = 20;
}

// ... use allocated memory ...

arena_free(arena);
```

### 5. Free Memory (Whole Arena at Once)

Arena allocators are designed for bulk deallocation.  To free all memory allocated from an arena, you typically **free or reset the entire arena**, not individual blocks:

**Free Dynamic Arena:**

```c
arena_free(arena); // Frees memory allocated by arena_new_dynamic
```

**Reset Arena (For both static and dynamic arenas):**

```c
arena_reset(arena); // Resets the arena, making all blocks free again, but without freeing the arena's memory itself.
```

### 6. Debugging with `print_arena` and `print_fancy`

To enable debugging output, compile your code with the `DEBUG` macro defined:

```bash
gcc -DDEBUG your_code.c arena_impl.c -o your_program
```

Then, you can call `print_arena(arena)` or `print_fancy(arena)` to print detailed information about the arena's state to the console, which can be helpful for debugging memory allocation issues.

### Customization

**`MIN_BUFFER_SIZE` Macro:**

You can adjust the `MIN_BUFFER_SIZE` macro in `arena_allocator.h` to control the minimum size of free blocks that are kept after splitting.  Experiment with different values to optimize for your specific use case.

## License

This library is licensed under the MIT License. See [LICENSE](LICENSE) for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

