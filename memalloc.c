#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "memalloc.h"

// ---------- Configurable Macros ----------
#define ALIGNMENT 8
#define USE_BEST_FIT 0   // 0 = First-Fit, 1 = Best-Fit
#define DEBUG 1          // 1 = Print debug info
#define CANARY 0xDEADBEEFCAFEBABEULL

#define align8(x) (((((x) - 1) >> 3) << 3) + ALIGNMENT)

#if DEBUG
#define debug_printf(...) printf(__VA_ARGS__)
#else
#define debug_printf(...) 
#endif

typedef struct block_header {
    size_t size;
    int free;
    struct block_header* next;
    unsigned long long canary;
} block_header;

static block_header* free_list = NULL;
static pthread_mutex_t global_malloc_lock = PTHREAD_MUTEX_INITIALIZER;
static size_t total_allocations = 0;
static size_t total_frees = 0;

// ---------- Internal Helper Functions ----------

void set_canary(block_header* block) {
    block->canary = CANARY;
}

int check_canary(block_header* block) {
    return block->canary == CANARY;
}

block_header* find_free_block(block_header** last, size_t size) {
    block_header* current = free_list;
#if USE_BEST_FIT
    block_header* best_fit = NULL;
    size_t smallest_diff = (size_t) -1;

    while (current) {
        if (current->free && current->size >= size) {
            size_t diff = current->size - size;
            if (diff < smallest_diff) {
                best_fit = current;
                smallest_diff = diff;
            }
        }
        *last = current;
        current = current->next;
    }
    return best_fit;
#else
    while (current && !(current->free && current->size >= size)) {
        *last = current;
        current = current->next;
    }
    return current;
#endif
}

block_header* request_space(block_header* last, size_t size) {
    block_header* block;
    block = sbrk(0);
    void* request = sbrk(size + sizeof(block_header));
    if (request == (void*)-1) {
        return NULL;
    }
    if (last) {
        last->next = block;
    }
    block->size = size;
    block->free = 0;
    block->next = NULL;
    set_canary(block);
    return block;
}

void split_block(block_header* block, size_t size) {
    if (block->size >= size + sizeof(block_header) + ALIGNMENT) {
        block_header* new_block = (block_header*)((char*)(block + 1) + size);
        new_block->size = block->size - size - sizeof(block_header);
        new_block->free = 1;
        new_block->next = block->next;
        set_canary(new_block);
        block->size = size;
        block->next = new_block;
    }
}

void coalesce_free_blocks() {
    block_header* current = free_list;
    while (current && current->next) {
        if (current->free && current->next->free) {
            current->size += sizeof(block_header) + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

void leak_check() {
    block_header* current = free_list;
    int leaks = 0;

    printf("\n[Leak Check]\n");
    while (current) {
        if (!current->free) {
            printf("Leaked block at %p, size %zu bytes\n", (void*)(current + 1), current->size);
            leaks++;
        }
        current = current->next;
    }
    if (leaks == 0) {
        printf("No memory leaks detected!\n");
    }
    printf("Total allocations: %zu, Total frees: %zu\n", total_allocations, total_frees);
}

// ---------- Allocation Functions ----------

void* malloc_custom(size_t size) {
    pthread_mutex_lock(&global_malloc_lock);

    if (size == 0) {
        pthread_mutex_unlock(&global_malloc_lock);
        return NULL;
    }

    size = align8(size);
    block_header* block;

    if (!free_list) {
        block = request_space(NULL, size);
        if (!block) {
            pthread_mutex_unlock(&global_malloc_lock);
            return NULL;
        }
        free_list = block;
    } else {
        block_header* last = free_list;
        block = find_free_block(&last, size);
        if (!block) {
            block = request_space(last, size);
            if (!block) {
                pthread_mutex_unlock(&global_malloc_lock);
                return NULL;
            }
        } else {
            block->free = 0;
            split_block(block, size);
        }
    }

    total_allocations++;
    debug_printf("[malloc] Allocated %zu bytes at %p\n", size, (void*)(block + 1));

    pthread_mutex_unlock(&global_malloc_lock);
    return (block + 1);
}

void free_custom(void* ptr) {
    if (!ptr) return;

    pthread_mutex_lock(&global_malloc_lock);

    block_header* block = (block_header*)ptr - 1;

    if (!check_canary(block)) {
        fprintf(stderr, "Error: Memory corruption detected during free at %p\n", ptr);
        pthread_mutex_unlock(&global_malloc_lock);
        abort();
    }

    if (block->free) {
        fprintf(stderr, "Error: Double free detected at %p\n", ptr);
        pthread_mutex_unlock(&global_malloc_lock);
        abort();
    }

    block->free = 1;
    total_frees++;
    debug_printf("[free] Freed memory at %p\n", ptr);
    coalesce_free_blocks();

    pthread_mutex_unlock(&global_malloc_lock);
}

void* calloc_custom(size_t num, size_t nsize) {
    size_t size = num * nsize;
    if (nsize != 0 && size / nsize != num) {
        return NULL; // Overflow
    }
    void* ptr = malloc_custom(size);
    if (!ptr) return NULL;
    memset(ptr, 0, size);
    return ptr;
}

void* realloc_custom(void* ptr, size_t size) {
    if (!ptr) {
        return malloc_custom(size);
    }
    if (size == 0) {
        free_custom(ptr);
        return NULL;
    }

    block_header* block = (block_header*)ptr - 1;
    if (!check_canary(block)) {
        fprintf(stderr, "Error: Memory corruption detected during realloc at %p\n", ptr);
        abort();
    }

    if (block->size >= size) {
        return ptr;  // Existing block is enough
    }

    void* new_ptr = malloc_custom(size);
    if (!new_ptr) return NULL;

    memcpy(new_ptr, ptr, block->size);
    free_custom(ptr);
    return new_ptr;
}

// ---------- Debugging Function ----------

void print_memory_blocks() {
    pthread_mutex_lock(&global_malloc_lock);

    block_header* current = free_list;
    printf("\n--- Memory Blocks ---\n");
    while (current) {
        printf("Block %p: size = %zu, free = %d, next = %p\n",
               (void*)current, current->size, current->free, (void*)current->next);
        current = current->next;
    }
    printf("---------------------\n");

    pthread_mutex_unlock(&global_malloc_lock);
}

// ---------- Register Leak Check ----------
__attribute__((constructor))
void init_memalloc() {
    atexit(leak_check);
}