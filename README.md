# Header-Only Arena-Based Memory Allocator in C

<!-- Look ma, I added badges! Now my toy is a "serious project"! -->
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![CodeFactor](https://www.codefactor.io/repository/github/gooderfreed/arena_c/badge)](https://www.codefactor.io/repository/github/gooderfreed/arena_c)
[![codecov](https://codecov.io/gh/gooderfreed/arena_c/graph/badge.svg?token=QJ3YND0OBF)](https://codecov.io/gh/gooderfreed/arena_c)
[![C Project CI](https://github.com/gooderfreed/arena_c/actions/workflows/ci.yml/badge.svg)](https://github.com/gooderfreed/arena_c/actions/workflows/ci.yml)
<!-- There. Shiny enough? Now let's get back to actual work. -->

**A fast, portable, and easy-to-use header-only arena-based memory allocator library written in pure C.**

This library serves as the core memory allocator for the [Zen Framework](https://github.com/gooderfreed/Zen) - a lightweight, modular framework for building console applications in C. The arena allocator provides efficient memory management for Zen's component-oriented architecture and interactive features.

## Overview

This library provides a header-only implementation of an arena-based memory allocator in C. Arena allocation is a memory management technique that allocates memory in large chunks (arenas) and then subdivides them for application use, which can offer significant performance benefits in scenarios with frequent allocations.

This implementation is designed with a focus on **performance, memory efficiency, and flexibility**, providing advanced features like nested arenas and bump sub-allocation within a robust, portable package.

**Key Benefits:**

*   **High Performance:** Fast allocations with O(log n) complexity for finding free blocks (via an LLRB-Tree) and O(1) for most tail allocations.
*   **Memory Efficiency:** Minimized metadata overhead and reduced memory fragmentation.
*   **Flexible Sub-allocation:**
    *   **Nested Arenas** for hierarchical memory management (`arena_new_nested`).
    *   **Bump Allocators** for extremely fast, zero-overhead allocation of many small objects (`bump_new`).
*   **Fine-grained Control:** Supports freeing individual blocks (`arena_free_block`) for memory reuse, in addition to fast full-arena resets.
*   **Source-Agnostic API:** The core functions operate on an `Arena*` handle, making them independent of the memory source. This allows for easy switching between static and dynamic arenas without altering the allocation logic.
*   **Simplicity:**  Easy to integrate into C projects as a header-only library.

## Recommended Use Cases

This library provides a flexible, high-performance alternative to standard `malloc`/`free` for a wide range of applications. It excels in scenarios requiring:
*   **High-throughput Allocations:** In systems that perform a large number of allocations and deallocations, such as game engines, network servers, or custom data structures. The O(1) tail allocation and bump sub-allocators are ideal for this.
*   **Controlled Memory Lifecycles:** When you can group objects into scopes and deallocate them all at once (e.g., per-frame data, per-request state, parsing a file). This is the classic arena use case, and it's extremely fast.
*   **Reduced Memory Fragmentation:** For long-running applications where standard allocator fragmentation can become a problem. The block-merging strategy helps keep the memory pool healthy.
*   **Fine-grained Memory Management:** When you need more control than `malloc` offers, but want to avoid writing a full custom allocator. The combination of individual `free`, bump allocators, and nested arenas provides a powerful toolkit.
*   **Simplified Multithreading:** In parallel systems using a "one-allocator-per-thread" model. The `arena_new_nested` function makes this pattern safe and easy to implement without locks.

## Features

*   **Header-Only & Portable:** Easy to integrate by including a single header. Continuously tested on Linux, macOS, Windows, x86_64, x86_32, and ARM64.

*   **Flexible Arena Creation:**
    *   **Static Arena:** Use pre-allocated memory (stack, global buffer).
    *   **Dynamic Arena:** Allocate the arena's memory from the heap.
    *   **Nested Arenas:** Safely create sub-arenas within a parent, ideal for thread-local storage.

*   **Advanced Allocation Strategies:**
    *   **General-Purpose Allocation:** An efficient free-list allocator with O(log n) performance using an LLRB-Tree and block merging to combat fragmentation.
    *   **Bump Sub-allocation:** Create zero-overhead bump allocators from any block for extremely fast, sequential allocations of small objects.
    *   **Optimized Tail Allocation:** Most allocations at the end of the arena are O(1).

*   **Memory Correctness & Efficiency:**
    *   **Automatic Memory Alignment:** All allocations are aligned by default to `16` bytes, ensuring compatibility with SIMD (SSE) instructions.
    *   **Minimal Metadata Overhead:** Only **32 bytes** per `Arena` and `Block` header, achieved via pointer tagging and optimized struct layout.

*   **Full Memory Control:**
    *   **Individual Block Freeing:** `arena_free_block` allows freeing blocks in any order, just like a standard allocator.
    *   **Fast Arena Reset:** `arena_reset` deallocates all blocks within an arena in O(1) time without freeing the arena's own memory.

*   **Excellent Developer Experience:**
    *   **Source-Agnostic API:** A single set of functions works on both static and dynamic arenas.
    *   **Powerful Debugging Tools:** Optional `print_arena` and `print_fancy` functions provide detailed, colorized visualizations of the arena's state.

*   **Customizable & Open:**
    *   **Tunable `MIN_BUFFER_SIZE`:** Adjust the trade-off between memory fragmentation and overhead.
    *   **Permissive MIT License:** Use it in any project.

## Important Considerations & Best Practices

*   **Metadata Overhead:** The allocator is highly optimized to minimize metadata overhead. Each `Arena` and `Block` header consumes only **32 bytes**. While this is very low, allocating a huge number of tiny, individual objects directly from the main arena can still be inefficient.
*   **Handling Many Small Objects:** For scenarios requiring a high volume of small, short-lived allocations, the recommended approach is to use a **Bump sub-allocator**. Create a single, larger block with `bump_new` and perform subsequent tiny allocations from it with near-zero overhead. This combines the flexibility of the main arena with the raw speed of a bump allocator.
*   **No `realloc` Equivalent:** This library does not provide a direct equivalent of `realloc`. Resizing a block would require moving its contents, which goes against the core design principles of an arena where object locations are stable. Plan your memory requirements accordingly or implement resizing at the application level.
*   **Memory Locality:** Allocating related objects within the same arena generally improves memory locality, as they are likely to be physically close in memory. This can lead to better cache performance compared to allocations scattered across the heap by a general-purpose allocator.

## Getting Started

### 1. Include the Header File

```c
#include "arena.h"
```

Make sure to copy `arena.h` into your project's include directory or adjust the include path accordingly.

### 2. Implement Arena Allocator Functions (Once per Project)

In **one** of your `.c` files (e.g., `arena_impl.c`), define the `ARENA_IMPLEMENTATION` macro **before** including `arena.h`:

```c
#define ARENA_IMPLEMENTATION
#include "arena.h"
```

**Important:**  Define `ARENA_IMPLEMENTATION` in **only one** C file in your project. This will compile the implementation of the arena allocator functions.  In all other files where you use the arena allocator, just include `arena.h` without defining `ARENA_IMPLEMENTATION`.

### Alternative Integration for Large Projects

In complex projects with intricate include hierarchies, ensuring that `ARENA_IMPLEMENTATION` is defined in exactly one `.c` file can be challenging. An alternative approach is to compile the header file directly into its own object file.

You can achieve this by adding a specific build rule to your build system (e.g., Makefile, CMake). Here's an example using `gcc`:

```bash
# Example Makefile rule
arena.o: arena.h
	gcc -x c -DARENA_IMPLEMENTATION -c arena.h -o arena.o
```

**Explanation:**

*   `gcc -x c`: This flag tells `gcc` to treat the input file (`arena.h`) as a C source file, even though it has a `.h` extension.
*   `-DARENA_IMPLEMENTATION`: This defines the necessary macro to include the function implementations.
*   `-c`: This tells `gcc` to compile the source file into an object file (`arena.o`) without linking.

Then, simply link the resulting `arena.o` object file with the rest of your project. This isolates the implementation into a single compilation unit, avoiding potential conflicts in large codebases.

### 3. Create an Arena

**Dynamic Arena (Memory allocated using `malloc`):**

```c
#include "arena.h"

int main() {
    ssize_t arena_size = 1024 * 1024; // 1MB arena
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
#include "arena.h"

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

### 5. Advanced Usage: Sub-allocation

The allocator provides powerful sub-allocation features for advanced use cases like thread-local storage or managing high volumes of small objects.

#### Using a Nested Arena

Nested arenas are perfect for creating temporary, scoped memory pools, such as for a single task in a thread pool or for processing one request in a server.

```c
void process_request(Arena *main_arena) {
    // Create a temporary arena for this specific request from the main arena.
    Arena *request_arena = arena_new_nested(main_arena, 1024 * 64); // 64KB for this task
    if (!request_arena) {
        // Not enough space in the main arena
        return;
    }

    // Perform all allocations for this request from the temporary arena.
    char *user_data = (char *)arena_alloc(request_arena, 100);
    int *session_ids = (int *)arena_alloc(request_arena, sizeof(int) * 256);
    // ... more allocations ...
    
    // Once the request is processed, free the entire nested arena in one go.
    // All memory allocated from request_arena is instantly returned to the main_arena.
    arena_free_nested(request_arena);
}
```

#### Using a Bump Allocator for Small Objects

When you need to allocate a very large number of small, short-lived objects (e.g., nodes in a graph, particles in a simulation), a bump allocator is the fastest method possible.

```c
typedef struct { float x, y, z; } Vector3;

void spawn_particles(Arena *main_arena) {
    // 1. Allocate a single large block and initialize it as a bump allocator.
    Bump *particle_memory = bump_new(main_arena, sizeof(Vector3) * 10000); // Space for 10k particles
    if (!particle_memory) {
        // Not enough space in the main arena
        return;
    }

    // 2. Perform extremely fast allocations from the bump allocator.
    // This is just incrementing a pointer, with near-zero overhead.
    Vector3 *particles[10000];
    for (int i = 0; i < 10000; ++i) {
        particles[i] = (Vector3 *)bump_alloc(particle_memory, sizeof(Vector3));
        if (!particles[i]) {
            // Bump allocator is full
            break; 
        }
        particles[i]->x = i; 
        // ... initialize particle ...
    }

    // ... use the particles ...

    // 3. When done, free the entire block back to the main arena.
    // All 10,000 allocations are freed in a single operation.
    bump_free(particle_memory);
}
```

### 6. Free Memory (Arena-Level Deallocation)

This allocator offers **multiple deallocation strategies**. You can free individual blocks using `arena_free_block` for fine-grained control, or deallocate all memory within the arena at once for maximum speed using:

**Free Dynamic Arena:**

```c
arena_free(arena); // Frees memory allocated by arena_new_dynamic, including all blocks within it.
```

**Reset Arena (For both static and dynamic arenas):**

```c
arena_reset(arena); // Resets the arena, marking all blocks as free and ready for reuse, but without freeing the arena's underlying memory itself.  Useful for reusing the arena for a new phase of allocation.
```

### 7. Debugging with `print_arena` and `print_fancy`

To enable debugging output, compile your code with the `DEBUG` macro defined:

```bash
gcc -DDEBUG your_code.c arena_impl.c -o your_program
```

Then, you can call `print_arena(arena)` or `print_fancy(arena, 100)` to print detailed information about the arena's state to the console, which can be helpful for debugging memory allocation issues and understanding arena behavior.

### Customization

**`MIN_BUFFER_SIZE` Macro:**

You can adjust the `MIN_BUFFER_SIZE` macro in `arena_allocator.h` to control the minimum size of free blocks that are kept after splitting.  Experiment with different values to optimize for your specific use case and balance memory usage with allocation speed.

## Build Status & Portability

The library is continuously tested across a wide range of operating systems, compilers, and architectures to ensure maximum reliability and portability.

### By Operating System

| OS      | Status                                                                                                                                                                                           |
|---------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Ubuntu  | ![Ubuntu Status](https://img.shields.io/github/actions/workflow/status/gooderfreed/arena_c/ci.yml?job=ubuntu-latest%20%7C%20x86_64%20%7C%20gcc&label=ubuntu&logo=ubuntu&logoColor=white)         |
| macOS   | ![macOS Status](https://img.shields.io/github/actions/workflow/status/gooderfreed/arena_c/ci.yml?job=macos-latest%20%7C%20x86_64%20%7C%20clang&label=macOS&logo=apple&logoColor=white)           |
| Windows | ![Windows Status](https://img.shields.io/github/actions/workflow/status/gooderfreed/arena_c/ci.yml?job=windows-latest%20%7C%20x86_64%20%7C%20gcc&label=windows&logo=windows&logoColor=white)     |

### By Compiler

| Compiler    | Status                                                                                                                                                                                          |
|-------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| GCC         | ![GCC Status](https://img.shields.io/github/actions/workflow/status/gooderfreed/arena_c/ci.yml?job=ubuntu-latest%20%7C%20x86_64%20%7C%20gcc&label=gcc&logo=gcc&logoColor=white)                 |
| GCC (MinGW) | ![GCC Status](https://img.shields.io/github/actions/workflow/status/gooderfreed/arena_c/ci.yml?job=windows-latest%20%7C%20x86_64%20%7C%20gcc&label=gcc%20(mingw)&logo=windows&logoColor=white)  |
| Clang       | ![Clang Status](https://img.shields.io/github/actions/workflow/status/gooderfreed/arena_c/ci.yml?job=ubuntu-latest%20%7C%20x86_64%20%7C%20clang&label=clang&logo=llvm&logoColor=white)          |
| MSVC        | ![MSVC Status](https://img.shields.io/github/actions/workflow/status/gooderfreed/arena_c/ci.yml?job=windows-latest%20%7C%20x86_64%20%7C%20gcc&label=msvc&logo=visualstudio&logoColor=white)     |

### By Architecture

| Architecture          | Alignment Mode | Status                                                                                                                                                                                                                         |
|-----------------------|----------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `x86_64` (64-bit)     | Standard       | ![x86_64 Status](https://img.shields.io/github/actions/workflow/status/gooderfreed/arena_c/ci.yml?job=ubuntu-latest%20%7C%20x86_64%20%7C%20gcc&label=x86_64&logo=intel&logoColor=white)                                        |
| `x86`    (32-bit)     | Standard       | ![x86_32 Status](https://img.shields.io/github/actions/workflow/status/gooderfreed/arena_c/ci.yml?job=Ubuntu%20%7C%20x86_32%20%7C%20GCC&label=x86&logo=intel&logoColor=white)                                                  |
| `ARM64` (AArch64)     | Forgiving      | ![ARM64 Modern Status](https://img.shields.io/github/actions/workflow/status/gooderfreed/arena_c/ci.yml?job=Ubuntu%20%7C%20ARM64%20(Modern,%20Forgiving%20Alignment)%20%7C%20GCC&label=arm&logo=arm&logoColor=white)           |
| `ARM64` (AArch64)     | Strict (UBSan) | ![ARM64 Strict Status](https://img.shields.io/github/actions/workflow/status/gooderfreed/arena_c/ci.yml?job=Ubuntu%20%7C%20ARM64%20(Strict%20Alignment%20via%20UBSan)%20%7C%20GCC&label=arm(ubsan)&logo=arm&logoColor=white)   |

## License

This library is licensed under the MIT License. See [LICENSE](LICENSE) for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
