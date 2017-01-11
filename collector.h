#ifndef COLLECTOR_H
#define COLLECTOR_H

#include <stddef.h>
#include <stdbool.h>

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