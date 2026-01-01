#include "allocator.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
int counter = 0;

static void basic_test(int verbose) {
    if (verbose) printf("== basic_test ==\n");
    char* a = (char*)my_malloc(20);
    char* b = (char*)my_malloc(40);
    if (!a || !b) {
        if (verbose) printf("alloc failed\n");
        return;
    }

    strcpy(a, "hello allocator");
    if (verbose) printf("a: %s\n", a);

    my_free(a);
    my_free(b);

    if (verbose) allocator_dump();
    else{
        pthread_mutex_lock(&lock);
        counter++;
        pthread_mutex_unlock(&lock);
    }
}

static void realloc_test(int verbose) {
    if (verbose) printf("== realloc_test ==\n");
    char* p = (char*)my_malloc(16);
    if (!p) return;
    strcpy(p, "ciao");
    if (verbose) printf("p: %s\n", p);

    p = (char*)my_realloc(p, 128);
    if (!p) {
        if (verbose) printf("realloc failed\n");
        return;
    }
    strcat(p, " mondo");
    if (verbose) printf("p after realloc: %s\n", p);

    my_free(p);
    if (verbose) allocator_dump();
    else{
        pthread_mutex_lock(&lock);
        counter++;
        pthread_mutex_unlock(&lock);
    }
}

static void calloc_test(int verbose) {
    if (verbose) printf("== calloc_test ==\n");
    int* v = (int*)my_calloc(10, sizeof(int));
    if (!v) return;
    int ok = 1;
    for (int i = 0; i < 10; i++) {
        if (v[i] != 0) ok = 0;
    }
    if (verbose) printf("calloc zeroed: %s\n", ok ? "yes" : "no");
    if(!verbose && ok) {
        pthread_mutex_lock(&lock);
        counter++;
        pthread_mutex_unlock(&lock);
    }
    
    my_free(v);
}

// Simple LCG to avoid rand() thread-safety issues
static int next_rand(unsigned int* seed) {
    *seed = *seed * 1103515245 + 12345;
    return (int)((*seed >> 16) & 0x7fff);
}

static void* thread_test(void* arg) {
    unsigned int seed = (unsigned int)(uintptr_t)arg;
    for (int i = 0; i < 100; i++) {
        int r = next_rand(&seed) % 3;

        if (r == 0) {
            basic_test(0);
        } else if (r == 1) {
            calloc_test(0);
        } else {
            realloc_test(0);
        }
    }
    return NULL;
}

void thread_safety_test() {
    printf("== thread_safety_test ==\n");
    int cpuN = 8;
    pthread_t threads[cpuN];
    for (int i = 0; i < cpuN; i++) {
        pthread_create(&threads[i], NULL, thread_test, (void*)(uintptr_t)i);
    }

    for (int i = 0; i < cpuN; i++) {
        pthread_join(threads[i], NULL);
    }
    printf("\nThread safety test passed: %d/800\n", counter);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <heap_size_bytes>\n", argv[0]);
        return 1;
    }

    size_t heap = (size_t)strtoull(argv[1], NULL, 10);
    if (allocator_init(heap) != 0) {
        perror("allocator_init");
        return 1;
    }

    basic_test(1);
    realloc_test(1);
    calloc_test(1);
    thread_safety_test();

    allocator_destroy();
    return 0;
}
