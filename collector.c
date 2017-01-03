#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include "collector.h"
#include "pointer_macros.h"

#ifdef DEBUG
#define DEBUG_TEST 1
#else
#define DEBUG_TEST 0
#endif

#define debug_print(...) do { if (DEBUG_TEST) fprintf(stderr, ##__VA_ARGS__); } while (0)

void *used_head = NULL;

/*
gc_malloc memory block layout:

+-----------------------------+-----------+-----------+
|          User data          | *previous |   *next   |
+-----------------------------+-----------+-----------+
*/

bool marked(void *block)
{
    return (uintptr_t) PREVIOUS_POINTER(block) & 1;
}

void mark_block(void *block)
{
    PREVIOUS_POINTER(block) = (void *) ((uintptr_t) PREVIOUS_POINTER(block) | 1);
}

void unmark_block(void *block)
{
    PREVIOUS_POINTER(block) = (void *) ((uintptr_t) PREVIOUS_POINTER(block) & -2);
}

void *gc_malloc(size_t size)
{
    // abort if size+2*POINTER_SIZE causes overflow
    assert(size < size+2*POINTER_SIZE);

    void *block = malloc(size+2*POINTER_SIZE);
    if (block == NULL) {
        debug_print("Malloc failure...\tSize request: %zu\n", size+2*POINTER_SIZE);
        return NULL;
    }
    PREVIOUS_POINTER(block) = &used_head; // point previous pointer at address of used_head
    NEXT_POINTER(block) = used_head; // point next pointer at address of next block
    if (used_head != NULL) { 
        PREVIOUS_POINTER(used_head) = block; // point previous pointer of next block at new block
    }
    used_head = block; // new block at beginning of linked list
    debug_print("Block allocated...\tAddress:%p\tSize: %zu\tPrevious: %p\tNext: %p\n", 
            block, malloc_usable_size(block), PREVIOUS_POINTER(block), NEXT_POINTER(block));
    return block;
}

void *gc_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
        return gc_malloc(size);
    }
    void *previous = PREVIOUS_POINTER(ptr);
    void *next = NEXT_POINTER(ptr);
    // if size request is 0, free block
    if (size == 0) {
        debug_print("Realloc size request 0, freeing block...\tAddress: %p\n", ptr);
        if (previous != &used_head) {
            NEXT_POINTER(previous) = next;
        } else {
            used_head = next;
        }
        if (next != NULL) {
            PREVIOUS_POINTER(next) = previous;
        }
        free(ptr);
        return NULL;
    }
    // clear previous and next pointers in original, small block
    PREVIOUS_POINTER(ptr) = (void *) 0;
    NEXT_POINTER(ptr) = (void *) 0;
    // request a new, larger block of memory. Abort if size+2*POINTER_SIZE causes overflow
    assert(size < size+2*POINTER_SIZE);
    void *new = realloc(ptr, size+2*POINTER_SIZE);
    // if realloc failed, return NULL
    if (new == NULL) {
        debug_print("Realloc failure...\tPointer: %p\tSize request: %zu\n", ptr, size+2*POINTER_SIZE);
        PREVIOUS_POINTER(ptr) = previous;
        NEXT_POINTER(ptr) = next;
        return NULL;
    }
    // set previous and next block pointers at end of larger block
    PREVIOUS_POINTER(new) = previous;
    NEXT_POINTER(new) = next;
    // if data was not moved simply return old pointer
    if (ptr == new) {
        debug_print("Block resized in place...\tAddress: %p\tSize: %zu\tPrevious: %p\tNext: %p\n",
                ptr, malloc_usable_size(new), previous, next);
        return ptr;
    }
    // if get here, data has been moved. Update previous and next block pointers
    if (previous != &used_head) {
        NEXT_POINTER(previous) = new;
    } else {
        used_head = new;
    }
    if (next != NULL) {
        PREVIOUS_POINTER(next) = new;
    }
    debug_print("Block resized, moved to new location...\tAddress: %p\tSize: %zu\tPrevious: %p\tNext: %p\n",
            new, malloc_usable_size(new), previous, next);
    return new;
}

void sweep(void) 
{
    debug_print("Commencing sweep...\n");
    void *previous = used_head;
    void *current = used_head;
    void *next;
    while (current != NULL && current != (void *) 1) {
        if (marked(current)) {
            debug_print("Skipping marked block...\tAddress: %p\n", current);
            unmark_block(current);
            next = NEXT_POINTER(current);
            previous = current;
        } else {
            next = NEXT_POINTER(current);
            if (used_head == current) {
                used_head = next;
            }
            debug_print("Freeing unmarked block...\tAddress: %p\n", current);
            if (next != NULL && next != (void *) 1) {
                bool mark_present = marked(next); 
                *((void **) next) = previous;
                if (mark_present) {
                    mark_block(next);
                }
            }
            NEXT_POINTER(previous) = next;
            free(current);
        }
        current = next;
    }
    debug_print("Sweep complete\n");
}

void print_heap(void)
{
    void *current = used_head;
    printf("Allocated blocks...\n");
    if (current == NULL) {
        printf("Heap empty\n");
    }
    while (current != NULL) {
        printf("Address: %p\tMarked: %d\n", current, marked(current));
        current = NEXT_POINTER(current);
    }
}