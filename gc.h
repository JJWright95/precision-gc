#ifndef GC_H
#define GC_H

#include <stddef.h>
#include <stdbool.h>

/*
gc_malloc memory block layout:

+-----------------------------+-----------+-----------+------------------+
|          User data          | *previous |   *next   | Stephen's Insert |
+-----------------------------+-----------+-----------+------------------+
                              ^
                        INSERT_ADDRESS
*/

#define POINTER_SIZE sizeof(void *)

#define INSERT_ADDRESS(BLOCK_ADDRESS) ((void *) ((char *) insert_for_chunk_and_usable_size(BLOCK_ADDRESS, malloc_usable_size(BLOCK_ADDRESS))  -2*POINTER_SIZE))

#define PREVIOUS_POINTER(BLOCK_ADDRESS) (*((void **) ((char *) insert_for_chunk_and_usable_size(BLOCK_ADDRESS, malloc_usable_size(BLOCK_ADDRESS)) - 2*POINTER_SIZE)))

#define NEXT_POINTER(BLOCK_ADDRESS) (*((void **) ((char *) insert_for_chunk_and_usable_size(BLOCK_ADDRESS, malloc_usable_size(BLOCK_ADDRESS)) -POINTER_SIZE)))

void gc_init(void);
bool marked(void *block);
void mark_block(void *block);
void unmark_block(void *block);
void *gc_malloc(size_t size);
void *gc_realloc(void *ptr, size_t size);
void *pointed_to_heap_block(void *pointer);
void scan_stack_for_pointers_to_heap(void);
void scan_data_segment_for_pointers_to_heap(void);
void scan_bss_segment_for_pointers_to_heap(void);
void sweep(void);
void print_heap(void);

#endif
