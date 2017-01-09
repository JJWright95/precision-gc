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
void sweep(void);
void print_heap(void);

#endif