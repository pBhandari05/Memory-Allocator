This project implements a custom memory allocator in C. It uses sbrk() to dynamically expand the program’s heap as needed. The allocator manages memory manually through a free list to track available blocks.

The allocator supports:
	•	malloc(size_t size): Allocates a block of memory of at least size bytes.
	•	free(void *ptr): Frees a previously allocated block, making it available for future allocations.
	•	realloc(void *ptr, size_t new_size): Resizes a previously allocated block.

I did do a little optimisation such as block coalescing, splitting, and alignment guarantees, which I talk about later.

⸻

Design Choices

1. Using sbrk() Instead of mmap() or Other Alternatives

I chose to use sbrk() because:
	•	Simple Heap Management: sbrk() moves the program’s break (the end of the process’s data segment), making it ideal for simple heap-like memory allocation systems.
	•	Contiguous Heap Space: sbrk() grows the heap in a contiguous manner, which is simple to manage with linked free lists.
	•	Performance: sbrk() incurs lower overhead compared to mmap(), which must interact with the operating system’s virtual memory management subsystem more deeply.
Most importantly, it was just easier. Maybe later I'll decide to use mmap in another iteration

Note: sbrk() is considered obsolete. Modern allocators (like glibc malloc) use mmap() internally for large allocations or fragmented heaps. However, for this project I didn't want too much complication.

⸻

2. Free List Management
	•	Linked List of Free Blocks: Each free block contains metadata (size, next pointer) and a payload. When memory is freed, the block is inserted into the free list.
	•	First Fit Policy: When allocating, the allocator scans the free list and picks the first block that is large enough. This is simple and efficient for smaller programs.
	•	Coalescing: When a block is freed, it is merged with adjacent free blocks if possible to reduce fragmentation and prevent the heap from growing unnecessarily.
	•	Splitting: If a free block is larger than needed, it is split into two: one part is allocated, and the remainder is put back into the free list (this could probably be made safer).

⸻

3. Alignment
	•	8-byte Alignment: All memory returned to the user is 8-byte aligned to satisfy typical architecture requirements for primitive types and improve performance on modern CPUs.

⸻

4. Error Handling
	•	Graceful Failure: If sbrk() fails (e.g., system out of memory), malloc returns NULL, and other functions handle this safely.
	•	Double Free Protection (basic): Minimal protection against common user mistakes (which I made) like double freeing the same pointer (could definitely be expanded).

How to use

void *ptr = malloc(100);   // allocate 100 bytes
ptr = realloc(ptr, 200);    // resize to 200 bytes
free(ptr);                 // free the memory

I used the following book to gain most of my understanding on the subject: https://gchandbook.org/
