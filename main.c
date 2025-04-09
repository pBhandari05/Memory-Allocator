#include <stdio.h>
#include "memalloc.h"

int main() {
    printf("Custom memory allocator test!\n");

    void* p1 = malloc_custom(100);
    void* p2 = malloc_custom(200);
    print_memory_blocks();

    free_custom(p1);
    print_memory_blocks();

    void* p3 = malloc_custom(50);
    print_memory_blocks();

    void* p4 = realloc_custom(p2, 300);
    print_memory_blocks();

    void* p5 = calloc_custom(10, 10);
    print_memory_blocks();

    free_custom(p3);
    free_custom(p4);
    free_custom(p5);
    print_memory_blocks();

    return 0;
}
