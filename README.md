<table>
  <tr>
    <td width="150" valign="middle">
      <img src=".github/assets/logo.png" width="150" alt="arena_c logo" />
    </td>
    <td valign="middle">
      <div id="user-content-toc">
        <ul style="list-style: none; padding: 0; margin: 0;">
          <summary>
            <h1 style="margin: 0;">Header-Only Arena Allocator in C</h1>
          </summary>
        </ul>
      </div>
      <p style="margin-top: 10px; margin-bottom: 0;">
        <a href="https://opensource.org/licenses/MIT"><img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License: MIT"></a>
        <a href="https://www.codefactor.io/repository/github/gooderfreed/arena_c"><img src="https://www.codefactor.io/repository/github/gooderfreed/arena_c/badge" alt="CodeFactor"></a>
        <a href="https://codecov.io/gh/gooderfreed/arena_c"><img src="https://codecov.io/gh/gooderfreed/arena_c/graph/badge.svg?token=QJ3YND0OBF" alt="codecov"></a>
        <a href="https://github.com/gooderfreed/arena_c/actions/workflows/ci.yml"><img src="https://github.com/gooderfreed/arena_c/actions/workflows/ci.yml/badge.svg" alt="Arena CI"></a>
      </p>
    </td>
  </tr>
</table>

<br/>

**An efficient, portable, and easy-to-use header-only arena allocator library written in pure C.**

## TL;DR

**What is it?** A high-performance, portable, header-only arena allocator for C.

**Key Features:**
*   **O(log n) Free List:** Uses a Left-Leaning Red-Black Tree for efficient block reuse and low fragmentation.
*   **Minimal Overhead:** Only 32 bytes of metadata per block (on 64-bit systems), achieved with pointer tagging and binary-compatible struct layouts.
*   **Nested Arenas:** Supports hierarchical memory management with zero-cost sub-arenas.
*   **Safe by Design:** Features compile-time assertions for configuration and runtime checks for pointer validity and magic number verification.
*   **100% Test Coverage:** Rigorously tested across multiple architectures and OSs.

**Why use it?** To get fast, controlled, and reliable memory management in performance-critical applications like game engines, embedded systems, or network servers.

**How to use it?** `#define ARENA_IMPLEMENTATION` in one `.c` file, then just `#include "arena.h"`.

This library serves as the core memory manager for the [Zen Framework](https://github.com/gooderfreed/Zen) - a lightweight, modular framework for building console applications in C.

## Overview

This library provides a header-only implementation of an arena allocator in C. Arena allocation is a memory management technique that allocates memory in large chunks and then subdivides them for application use, offering significant performance benefits over standard `malloc`/`free`.

This implementation focuses on **performance, memory efficiency, and robustness**, providing advanced features like nested arenas within a portable package.

**Key Benefits:**

*   **High Performance:** Fast allocations with O(log n) complexity for finding free blocks (via an LLRB-Tree) and O(1) for tail allocations.
*   **Memory Efficiency:** Minimized metadata overhead and optimized block merging to combat fragmentation.
*   **Flexible Creation:** Supports both **Static Arenas** (using pre-allocated buffers/stack) and **Dynamic Arenas** (heap-based).
*   **Nested Arenas:** Allows creating sub-pools within a parent arena for scoped memory management.
*   **Fine-grained Control:** Unlike many simple arenas, this one supports freeing individual blocks (`arena_free_block`) for memory reuse without resetting the entire arena.
*   **Source-Agnostic API:** Operates on an `Arena*` handle, making the allocation logic independent of whether the memory source is static or dynamic.

## Recommended Use Cases

*   **High-throughput Allocations:** Systems performing frequent allocations where `malloc` overhead is too high.
*   **Controlled Memory Lifecycles:** Grouping objects into scopes and deallocating them all at once (e.g., per-frame data, per-request state).
*   **Fragmentation-Sensitive Apps:** Long-running applications where standard allocator fragmentation becomes a problem over time.
*   **Simplified Multithreading:** Using a "one-allocator-per-thread" model. `arena_new_nested` makes this pattern safe and easy to implement.

## Features

*   **Extreme Portability:** Header-only and tested on Linux, macOS, and Windows; x86, ARM64, and ARM32; and both **Little Endian and Big Endian** orders.
*   **Full C++ Compatibility:** Wrapped in `extern "C"` for seamless integration.
*   **Advanced Allocation:** General-purpose free-list allocator with O(log n) performance and block merging.
*   **Optimized Tail Allocation:** Most allocations at the end of the arena are O(1).
*   **Memory Correctness:** Automatic 16-byte alignment by default (optimized for SIMD).
*   **Minimal Metadata:** Optimized struct layout using pointer tagging to keep headers small.
*   **Full Control:** Standard-like interface (`arena_alloc`, `arena_calloc`, `arena_free_block`) plus fast `arena_reset`.
*   **Powerful Debugging:** Colorized terminal visualization of memory state via `print_fancy`.

## Architectural Philosophy

Memory management involves a trade-off between **cache locality**, **pointer stability**, and **resizability**. You can only pick two.

This library prioritizes **locality and stability**.

![Tradeoffs](.github/assets/design_tradeoffs.png)

### Principle 1: Pointers are Stable (No `realloc`)
Once memory is allocated, its address never changes. This allows you to safely store pointers to allocated objects without worrying about invalidation. Consequently, there is no `realloc`; resizing requires allocating a new block and copying.

### Principle 2: Memory is Local (Performance by Default)
Allocations happen in contiguous chunks. This dramatically improves cache performance compared to the "scattered" nature of standard `malloc`.

## Getting Started

### 1. Include and Implement
In **one** `.c` file:
```c
#define ARENA_IMPLEMENTATION
#include "arena.h"
```

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


### 2. Standard Usage
```c
// Create a 1MB dynamic arena with default 16-byte alignment
Arena *arena = arena_new_dynamic(1024 * 1024);

// Standard allocation (returns 16-byte aligned pointer)
int *data = (int *)arena_alloc(arena, sizeof(int) * 100);

// Zero-initialized allocation
Point *pts = (Point *)arena_calloc(arena, 10, sizeof(Point));

// Free individual block (optional, allows internal reuse)
arena_free_block(data);

// Reset arena (clears all blocks in O(1), memory remains allocated)
arena_reset(arena);

// Full cleanup
arena_free(arena);
```

### 3. Custom Baseline Alignment & Static Memory
You can initialize an arena with a custom baseline alignment. **All** subsequent standard `arena_alloc` calls will automatically respect this alignment.

```c
// Use pre-allocated buffer with a strict 64-byte baseline alignment (e.g., for AVX-512)
char buffer[4096];
Arena *static_arena = arena_new_static_custom(buffer, sizeof(buffer), 64);

// This pointer is guaranteed to be 64-byte aligned
void *p = arena_alloc(static_arena, 256);
```

### 4. Overriding Alignment per Allocation
If you need a specific block to have stricter alignment than the arena's baseline (e.g., aligning a single buffer to a page boundary), use `arena_alloc_custom`.

```c
Arena *arena = arena_new_dynamic(1024 * 1024); // Baseline is 16

// Standard allocation (16-byte aligned)
void *p1 = arena_alloc(arena, 100);

// Specific allocation with 128-byte alignment
void *p2 = arena_alloc_custom(arena, 512, 128);
```

### 5. Nested Arenas (Scoped Memory)
Nested arenas allow you to create temporary sub-pools. Freeing a nested arena instantly returns its entire memory block to the parent.

```c
void process_request(Arena *main_arena) {
    // Create a 64KB sub-arena from the main arena
    Arena *request_scope = arena_new_nested(main_arena, 1024 * 64);
    
    // All allocations in this scope are freed at once below
    void *tmp = arena_alloc(request_scope, 1024);
    
    arena_free(request_scope); 
}
```

## Configuration

Define these macros **before** including `arena.h` to customize behavior:

| Macro | Default | Description |
| :--- | :--- | :--- |
| **`ARENA_DEFAULT_ALIGNMENT`** | `16` | Minimum allocation alignment (must be power of two). |
| **`ARENA_MIN_BUFFER_SIZE`** | `16` | Minimum size of a free block split. |
| **`ARENA_POISONING`** | *Auto* | Fills freed memory with `0xDD` in DEBUG builds. |
| **`ARENA_NO_MALLOC`** | *Unset* | Disables `malloc`/`free` dependencies (for static-only use). |

## Build Status & Portability

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
| Architecture | Endianness | Status |
| :--- | :--- | :--- |
| `x86_64` | Little | ![x86_64 Status](https://img.shields.io/github/actions/workflow/status/gooderfreed/arena_c/ci.yml?branch=main&job=build-and-test-x86_64&label=x86_64&logo=intel&logoColor=white) |
| `x86_32` | Little | ![x86_32 Status](https://img.shields.io/github/actions/workflow/status/gooderfreed/arena_c/ci.yml?branch=main&job=build-and-test-32bit&label=x86_32&logo=intel&logoColor=white) |
| `AArch64` | Little | ![ARM64 Modern Status](https://img.shields.io/github/actions/workflow/status/gooderfreed/arena_c/ci.yml?branch=main&job=build-and-test-arm64-modern&label=aarch64&logo=arm&logoColor=white) |
| `ARMv7`  | Little     | ![ARM32 Status](https://img.shields.io/github/actions/workflow/status/gooderfreed/arena_c/ci.yml?job=Ubuntu%20%7C%20ARM32%20(armv7)%20%7C%20GCC&label=armv7&logo=arm&logoColor=white)                                        |
| `s390x` | **Big** | ![Big Endian Status](https://img.shields.io/github/actions/workflow/status/gooderfreed/arena_c/ci.yml?branch=main&job=build-and-test-big-endian&label=s390x&logo=ibm&logoColor=white) |

### C Standards Compliance
| Standard | Status |
| :--- | :--- |
| **C99 / C11 / C17 / C23** | ![C Standards](https://img.shields.io/github/actions/workflow/status/gooderfreed/arena_c/ci.yml?branch=main&job=build-and-test-C-stds&label=Passed) |

## Why All This?
*idk, i was bored*

## License
MIT License. See [LICENSE](LICENSE) for details.