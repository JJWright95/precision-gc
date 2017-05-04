#ifndef GC_H
#define GC_H

#include <stddef.h>
#include <stdbool.h>

size_t mymalloc_usable_size(void*);
void *mymalloc(size_t);
/*
gc_malloc memory block layout:

+-----------------------------+-----------+-----------+------------------+
|          User data          | *previous |   *next   | Stephen's Insert |
+-----------------------------+-----------+-----------+------------------+
                              ^
                        INSERT_ADDRESS
*/

#define POINTER_SIZE sizeof(void *)

#define INSERT_ADDRESS(BLOCK_ADDRESS) ((void *) ((char *) insert_for_chunk_and_usable_size(BLOCK_ADDRESS, mymalloc_usable_size(BLOCK_ADDRESS))  -2*POINTER_SIZE))

#define PREVIOUS_POINTER(BLOCK_ADDRESS) (*((void **) ((char *) insert_for_chunk_and_usable_size(BLOCK_ADDRESS, mymalloc_usable_size(BLOCK_ADDRESS)) - 2*POINTER_SIZE)))

#define NEXT_POINTER(BLOCK_ADDRESS) (*((void **) ((char *) insert_for_chunk_and_usable_size(BLOCK_ADDRESS, mymalloc_usable_size(BLOCK_ADDRESS)) -POINTER_SIZE)))

#define GC_INIT gc_init
#define GC_MALLOC gc_malloc
#define GC_REALLOC gc_realloc
#define GC_NEW(OBJECT) (GC_MALLOC(sizeof(OBJECT)))
#define GC_MALLOC_ATOMIC GC_MALLOC

void gc_init(void);
bool marked(void *block);
void mark_block(void *block);
void unmark_block(void *block);
void *gc_malloc(size_t size);
void *gc_realloc(void *ptr, size_t size);
bool points_to_heap_object(void *pointer);
void scan_stack_for_pointers_to_heap(void);
void scan_data_segment_for_pointers_to_heap(void);
void scan_bss_segment_for_pointers_to_heap(void);
void sweep(void);
void gc_collect(void);
void print_heap(void);
int GC_num_collections(void);
long GC_heap_size(void);

#endif
