#define _GNU_SOURCE
#include "allocator.h"

#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>

typedef struct block_header {
    size_t size;               // payload size in bytes
    int free;                  // 1 if free, 0 if allocated
    struct block_header* next;
    struct block_header* prev;
} block_header_t;

static void* g_heap_start = NULL;
static size_t g_heap_size = 0;
static block_header_t* g_head = NULL;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static size_t page_align_up(size_t n) {
    long ps = sysconf(_SC_PAGESIZE);
    size_t page = (ps > 0) ? (size_t)ps : 4096u;
    return (n + page - 1) & ~(page - 1);
}

static size_t align_up(size_t n) {
    const size_t A = 16u; // practical alignment
    return (n + A - 1) & ~(A - 1);
}

static inline void* payload_from_header(block_header_t* h) {
    return (void*)((uint8_t*)h + sizeof(block_header_t));
}

static inline block_header_t* header_from_payload(void* p) {
    return (block_header_t*)((uint8_t*)p - sizeof(block_header_t));
}

static block_header_t* find_free_block(size_t size) {
    for (block_header_t* cur = g_head; cur != NULL; cur = cur->next) {
        if (cur->free && cur->size >= size) {
            return cur;
        }
    }
    return NULL;
}

static void split_block(block_header_t* b, size_t wanted) {
    const size_t min_payload = 16u;
    if (b->size < wanted + sizeof(block_header_t) + min_payload) return;

    uint8_t* base = (uint8_t*)b;
    block_header_t* newb = (block_header_t*)(base + sizeof(block_header_t) + wanted);

    newb->size = b->size - wanted - sizeof(block_header_t);
    newb->free = 1;
    newb->next = b->next;
    newb->prev = b;

    if (b->next) b->next->prev = newb;
    b->next = newb;

    b->size = wanted;
}

static void coalesce(block_header_t* b) {
    while (b->next && b->next->free) {
        block_header_t* n = b->next;
        b->size += sizeof(block_header_t) + n->size;
        b->next = n->next;
        if (b->next) b->next->prev = b;
    }

    if (b->prev && b->prev->free) {
        block_header_t* p = b->prev;
        p->size += sizeof(block_header_t) + b->size;
        p->next = b->next;
        if (p->next) p->next->prev = p;
        coalesce(p);
    }
}

int allocator_init(size_t heap_size) {
    if (g_heap_start) return 0;

    if (heap_size < sizeof(block_header_t) + 16u) {
        heap_size = sizeof(block_header_t) + 16u;
    }

    heap_size = page_align_up(heap_size);

    void* region = mmap(NULL, heap_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (region == MAP_FAILED) {
        return -1;
    }

    g_heap_start = region;
    g_heap_size = heap_size;

    g_head = (block_header_t*)g_heap_start;
    g_head->size = g_heap_size - sizeof(block_header_t);
    g_head->free = 1;
    g_head->next = NULL;
    g_head->prev = NULL;

    return 0;
}

void allocator_destroy(void) {
    if (!g_heap_start) return;
    munmap(g_heap_start, g_heap_size);
    g_heap_start = NULL;
    g_heap_size = 0;
    g_head = NULL;
}

void* my_malloc(size_t size) {
    if (size == 0) return NULL;

    pthread_mutex_lock(&lock);

    if (!g_heap_start) {
        errno = ENODEV;
        pthread_mutex_unlock(&lock);
        return NULL;
    }

    size = align_up(size);

    block_header_t* b = find_free_block(size);
    if (!b) {
        errno = ENOMEM;
        pthread_mutex_unlock(&lock);
        return NULL;
    }

    split_block(b, size);
    b->free = 0;
    pthread_mutex_unlock(&lock);
    return payload_from_header(b);
}

// Function only for the realloc that already has a lock
static void* my_malloc_locked(size_t size) {
    if (size == 0) return NULL;

    pthread_mutex_lock(&lock);

    if (!g_heap_start) {
        errno = ENODEV;
        pthread_mutex_unlock(&lock);
        return NULL;
    }

    size = align_up(size);

    block_header_t* b = find_free_block(size);
    if (!b) {
        errno = ENOMEM;
        pthread_mutex_unlock(&lock);
        return NULL;
    }

    split_block(b, size);
    b->free = 0;
    pthread_mutex_unlock(&lock);
    return payload_from_header(b);
}

void my_free(void* ptr) {
    if (!ptr) return;
    if (!g_heap_start) return;

    pthread_mutex_lock(&lock);
    uint8_t* p = (uint8_t*)ptr;
    uint8_t* start = (uint8_t*)g_heap_start;
    uint8_t* end = start + g_heap_size;
    if (p < start + sizeof(block_header_t) || p >= end){
        pthread_mutex_unlock(&lock);
        return;
    };

    block_header_t* b = header_from_payload(ptr);
    b->free = 1;
    coalesce(b);
    pthread_mutex_unlock(&lock);
}

void my_free_locked(void* ptr) {
    if (!ptr) return;
    if (!g_heap_start) return;

    pthread_mutex_lock(&lock);
    uint8_t* p = (uint8_t*)ptr;
    uint8_t* start = (uint8_t*)g_heap_start;
    uint8_t* end = start + g_heap_size;
    if (p < start + sizeof(block_header_t) || p >= end){
        pthread_mutex_unlock(&lock);
        return;
    };

    block_header_t* b = header_from_payload(ptr);
    b->free = 1;
    coalesce(b);
    pthread_mutex_unlock(&lock);
}

void* my_calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) return NULL;

    if (nmemb > (SIZE_MAX / size)) {
        errno = ENOMEM;
        return NULL;
    }

    size_t total = nmemb * size;
    void* p = my_malloc(total);
    if (!p) return NULL;
    memset(p, 0, total);
    return p;
}

//TODO: add my_malloc_locked
void* my_realloc(void* ptr, size_t new_size) {
    if (!g_heap_start) {
        errno = ENODEV;
        return NULL;
    }

    pthread_mutex_lock(&lock);
    if (!ptr){
        void* temp = my_malloc_locked(new_size);
        pthread_mutex_unlock(&lock);
        return temp;
    }

    if (new_size == 0) {
        my_free_locked(ptr);
        pthread_mutex_unlock(&lock);
        return NULL;
    }

    new_size = align_up(new_size);

    block_header_t* b = header_from_payload(ptr);
    if (b->size >= new_size) {
        split_block(b, new_size);
        pthread_mutex_unlock(&lock);
        return ptr;
    }

    if (b->next && b->next->free) {
        size_t combined = b->size + sizeof(block_header_t) + b->next->size;
        if (combined >= new_size) {
            block_header_t* n = b->next;
            b->size = combined;
            b->next = n->next;
            if (b->next) b->next->prev = b;

            split_block(b, new_size);
            pthread_mutex_unlock(&lock);
            return payload_from_header(b);
        }
    }

    void* newp = my_malloc(new_size);
    if (!newp) {
        pthread_mutex_unlock(&lock);
        return NULL;
    }

    memcpy(newp, ptr, b->size);
    my_free(ptr);
    pthread_mutex_unlock(&lock);
    return newp;
}

void allocator_dump(void) {
    if (!g_heap_start) {
        printf("[allocator] not initialized\n");
        return;
    }

    pthread_mutex_lock(&lock);
    printf("[allocator] heap=%p size=%zu\n", g_heap_start, g_heap_size);
    int i = 0;
    for (block_header_t* cur = g_head; cur; cur = cur->next, ++i) {
        printf("  #%d hdr=%p payload=%p size=%zu free=%d next=%p prev=%p\n",
               i, (void*)cur, payload_from_header(cur), cur->size, cur->free,
               (void*)cur->next, (void*)cur->prev);
    }
    pthread_mutex_unlock(&lock);
}
