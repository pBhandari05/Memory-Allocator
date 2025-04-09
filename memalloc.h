#ifndef MEMALLOC_H
#define MEMALLOC_H

#include <stddef.h>

void* malloc_custom(size_t size);
void free_custom(void* ptr);
void* calloc_custom(size_t num, size_t size);
void* realloc_custom(void* ptr, size_t size);
void print_memory_blocks();

#endif // MEMALLOC_H