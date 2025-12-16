#include "allocator.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void basic_test(void) {
    printf("== basic_test ==\n");
    char* a = (char*)my_malloc(20);
    char* b = (char*)my_malloc(40);
    if (!a || !b) {
        printf("alloc failed\n");
        return;
    }

    strcpy(a, "hello allocator");
    printf("a: %s\n", a);

    my_free(a);
    my_free(b);

    allocator_dump();
}

static void realloc_test(void) {
    printf("== realloc_test ==\n");
    char* p = (char*)my_malloc(16);
    if (!p) return;
    strcpy(p, "ciao");
    printf("p: %s\n", p);

    p = (char*)my_realloc(p, 128);
    if (!p) {
        printf("realloc failed\n");
        return;
    }
    strcat(p, " mondo");
    printf("p after realloc: %s\n", p);

    my_free(p);
    allocator_dump();
}

static void calloc_test(void) {
    printf("== calloc_test ==\n");
    int* v = (int*)my_calloc(10, sizeof(int));
    if (!v) return;
    int ok = 1;
    for (int i = 0; i < 10; i++) {
        if (v[i] != 0) ok = 0;
    }
    printf("calloc zeroed: %s\n", ok ? "yes" : "no");
    my_free(v);
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

    basic_test();
    realloc_test();
    calloc_test();

    allocator_destroy();
    return 0;
}
