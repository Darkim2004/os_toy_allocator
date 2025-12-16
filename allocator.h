#ifndef TOY_ALLOCATOR_H
#define TOY_ALLOCATOR_H

#include <stddef.h>   // size_t

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Educational heap allocator backed by a single mmap() region.
 *
 * Notes / limitations:
 * - Not thread-safe
 * - Single heap region (no growth via sbrk/brk)
 * - Intended for coursework / learning
 */

/** Initialize allocator with a heap of (at least) heap_size bytes. Returns 0 on success. */
int allocator_init(size_t heap_size);

/** Destroy allocator and release resources. Safe to call multiple times. */
void allocator_destroy(void);

/** malloc-like allocation. Returns NULL on failure. */
void* my_malloc(size_t size);

/** free-like deallocation. Safe to pass NULL. */
void my_free(void* ptr);

/** calloc-like allocation (nmemb * size bytes), zero-initialized. Returns NULL on failure. */
void* my_calloc(size_t nmemb, size_t size);

/** realloc-like reallocation. Returns NULL on failure; original pointer remains valid in that case. */
void* my_realloc(void* ptr, size_t new_size);

/** Optional: print the heap blocks to stdout (for debugging). */
void allocator_dump(void);

#ifdef __cplusplus
}
#endif

#endif // TOY_ALLOCATOR_H
