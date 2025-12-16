# os_toy_allocator (v0.1.0)

Educational heap allocator written in C, backed by a single `mmap()` region.
Implements simplified versions of `malloc`, `free`, `calloc`, and `realloc` for learning purposes.

## Features

- Fixed-size heap allocated with `mmap()`
- Doubly-linked list of blocks stored inside the heap
- Block splitting on allocation
- Coalescing (merge) of adjacent free blocks on `free`
- `realloc` tries to grow in-place when the next block is free; otherwise it allocates + copies + frees

## Build & run

Requirements: Linux / POSIX environment, `gcc` (or `clang`), `make`.

```bash
make
./tests 65536
```

You can pass the heap size (in bytes) as the first command-line argument.

## Project structure

- `allocator.c` / `allocator.h`: allocator implementation and public API
- `tests.c`: small test program + heap dump helper
- `Makefile`: build rules

## Notes / limitations

This repository contains **educational toy code**:

- Not thread-safe (no locking)
- Single fixed-size heap (no heap growth)
- Basic sanity checks only (no full corruption detection)
- Minimal alignment handling (sufficient for the demo, not production-grade)

## Example output (optional)

Running the tests prints a short log and a dump of the internal block list (headers, payload addresses, sizes, and free flags).
