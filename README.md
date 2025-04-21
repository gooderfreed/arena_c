# Header-Only Arena Allocator in C

<!-- Look ma, I added badges! Now my toy is a "serious project"! -->
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![CodeFactor](https://www.codefactor.io/repository/github/gooderfreed/arena_c/badge)](https://www.codefactor.io/repository/github/gooderfreed/arena_c)
[![codecov](https://codecov.io/gh/gooderfreed/arena_c/graph/badge.svg?token=QJ3YND0OBF)](https://codecov.io/gh/gooderfreed/arena_c)
[![C Project CI](https://github.com/gooderfreed/arena_c/actions/workflows/ci.yml/badge.svg)](https://github.com/gooderfreed/arena_c/actions/workflows/ci.yml)
<!-- There. Shiny enough? Now let's get back to actual work. -->

**A fast, efficient, and easy-to-use header-only arena memory allocator library written in pure C.**

## Overview

This library provides a header-only implementation of an arena memory allocator in C. Arena allocation is a memory management technique that allocates memory in large chunks (arenas) and then subdivides those chunks into smaller blocks for application use. This approach offers significant performance benefits and control over memory allocation and deallocation, especially in scenarios with frequent allocations and deallocations of objects of similar lifecycles.

**Key Benefits of Arena Allocation:**

*   **Performance:** Logarithmic O(log n) time complexity for allocations using a Left-Leaning Red-Black Tree (LLRB) for managing free blocks, significantly faster than general-purpose allocators like `malloc`/`free`.
*   **Efficiency:** Reduced memory fragmentation and smaller metadata overhead, leading to better memory utilization within the arena.
*   **Control:**  Deterministic deallocation - freeing the entire arena at once is very fast and predictable. Individual blocks can also be freed for memory reuse within the arena.
*   **Simplicity:**  Easy to integrate into C projects as a header-only library.

**When to Use Arena Allocation:**

*   Game development:  For managing game objects, particles, and temporary data that are often created and destroyed together.
*   Real-time systems:  Where predictable allocation and deallocation times are crucial for performance consistency.
*   Parsing and data processing:  For managing temporary data structures during parsing or processing large datasets that can be efficiently deallocated in bulk.
*   Any application where you need to allocate and deallocate many objects, especially of similar sizes and lifecycles, in a controlled and efficient manner.

## Features

*   **Header-Only Library:**  Easy integration - just include `arena_allocator.h` in your project.
*   **Static and Dynamic Arena Creation:**
    *   **Static Arena:**  Create arenas within pre-allocated memory regions (e.g., on the stack or in a global buffer) for maximum control and predictability.
    *   **Dynamic Arena:**  Dynamically allocate arena memory from the heap using `malloc`.
*   **Optimized Allocation Strategy:** 
    *   **Free Block Allocation:** Uses a balanced LLRB tree to find the best fitting block with O(log n) complexity
    *   **Tail Allocation:** Falls back to allocating from the tail when no suitable free blocks are available
*   **Block-Level Deallocation (`arena_free_block`):**  Allows freeing individual blocks within the arena, enabling memory reuse and finer-grained control over memory management beyond full arena resets.
*   **Memory Efficiency:**  Reduces fragmentation and improves memory utilization, especially when managing many objects within a defined memory scope.
*   **Arena Reset:**  Quickly reset the entire arena, marking all allocated blocks as free for reuse, offering a fast way to bulk-deallocate memory.
*   **Debugging Tools:**  Includes `print_arena` and `print_fancy` functions (enabled via `DEBUG` macro) for detailed arena state inspection and visualization during development.
*   **Customizable Minimum Block Size:**  Adjust `MIN_BUFFER_SIZE` macro in `arena_allocator.h` to fine-tune memory usage and fragmentation behavior based on your application's specific allocation patterns.
*   **Embedded Arena Metadata:** The arena's metadata structures are embedded directly within the allocated memory block, minimizing external dependencies but slightly reducing the total usable memory within the arena.
*   **Clear and Well-Commented Code:**  Easy to understand and modify.
*   **MIT License:**  Permissive open-source license.

**Important Considerations:**

*   **Arena Metadata Overhead:**  The arena metadata (e.g., block headers) consumes a small portion of the allocated arena memory. While optimized to only 48 bytes per block header, this overhead can become significant if you allocate a very large number of extremely small, individual objects separately. **It is NOT RECOMMENDED to use arena allocation for scenarios requiring allocation of a vast quantity of tiny, independent objects.**  For optimal efficiency, arena allocation is best suited for managing larger objects or groups of related objects with similar lifecycles.
*   **Memory Locality:** Arena allocation can improve memory locality, as objects allocated within the same arena are likely to be physically close in memory, potentially improving cache performance.

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

### Alternative Integration for Large Projects

In complex projects with intricate include hierarchies, ensuring that `ARENA_IMPLEMENTATION` is defined in exactly one `.c` file can be challenging. An alternative approach is to compile the header file directly into its own object file.

You can achieve this by adding a specific build rule to your build system (e.g., Makefile, CMake). Here's an example using `gcc`:

```bash
# Example Makefile rule
arena.o: arena_allocator.h
	gcc -x c -DARENA_IMPLEMENTATION -c arena_allocator.h -o arena.o
```

**Explanation:**

*   `gcc -x c`: This flag tells `gcc` to treat the input file (`arena_allocator.h`) as a C source file, even though it has a `.h` extension.
*   `-DARENA_IMPLEMENTATION`: This defines the necessary macro to include the function implementations.
*   `-c`: This tells `gcc` to compile the source file into an object file (`arena.o`) without linking.

Then, simply link the resulting `arena.o` object file with the rest of your project. This isolates the implementation into a single compilation unit, avoiding potential conflicts in large codebases.

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

### 4. Allocate and Free Memory Blocks from the Arena

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

// Free individual blocks when no longer needed (optional, but allows memory reuse within the arena)
arena_free_block(arena, my_int);
arena_free_block(arena, my_point);

// Allocate memory again, potentially reusing freed blocks
int *my_int_reused = (int *)arena_alloc(arena, sizeof(int));
if (my_int_reused) {
    *my_int_reused = 100;
}


// ... use allocated memory ...

arena_free(arena); // Free the entire arena when done
```

### 5. Free Memory (Arena-Level Deallocation)

Arena allocators are primarily designed for bulk deallocation.  You can choose to free individual blocks using `arena_free_block` for memory reuse, or you can efficiently deallocate all memory associated with the arena at once using:

**Free Dynamic Arena:**

```c
arena_free(arena); // Frees memory allocated by arena_new_dynamic, including all blocks within it.
```

**Reset Arena (For both static and dynamic arenas):**

```c
arena_reset(arena); // Resets the arena, marking all blocks as free and ready for reuse, but without freeing the arena's underlying memory itself.  Useful for reusing the arena for a new phase of allocation.
```

### 6. Debugging with `print_arena` and `print_fancy`

To enable debugging output, compile your code with the `DEBUG` macro defined:

```bash
gcc -DDEBUG your_code.c arena_impl.c -o your_program
```

Then, you can call `print_arena(arena)` or `print_fancy(arena, 100)` to print detailed information about the arena's state to the console, which can be helpful for debugging memory allocation issues and understanding arena behavior.

### Customization

**`MIN_BUFFER_SIZE` Macro:**

You can adjust the `MIN_BUFFER_SIZE` macro in `arena_allocator.h` to control the minimum size of free blocks that are kept after splitting.  Experiment with different values to optimize for your specific use case and balance memory usage with allocation speed.

## License

This library is licensed under the MIT License. See [LICENSE](LICENSE) for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
