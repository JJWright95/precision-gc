#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>

#include "heap_index.h"
#include "gc.h"
#include "pointer_macros.h"


#ifdef DEBUG
#define DEBUG_TEST 1
#else
#define DEBUG_TEST 0
#endif

#define debug_print(...) do { if (DEBUG_TEST) fprintf(stderr, ##__VA_ARGS__); } while (0)

/* The garbage collected heap is stored as a doubly linked list */
void *heap_list_head = NULL;
void *stack_base = NULL;

// look at linker scripts or symbol table for segment details
extern char data_start; // beginning of .data segment 
extern char edata; // end of .data segment
extern char __bss_start; // start of .bss segment
extern char end; // end of .bss segment

// heap-object queue data
struct q_node {
    void *heap_object;
    struct q_node *next;
};

struct q_node *q_head = NULL;
struct q_node *q_tail = NULL;

void enqueue_heap_queue_node(void *heap_pointer)
{
    struct q_node *node = malloc(sizeof(struct q_node));
    node->heap_object = heap_pointer;
    node->next = NULL;
    if (q_tail == NULL) {
        assert(q_head == NULL);
        q_tail = node;
        q_head = node;
    } else {
        q_tail->next = node;
        q_tail = node;
    }
}

struct q_node *dequeue_heap_queue_node(void)
{
    struct q_node *old_head = q_head;
    if (old_head != NULL) {
        q_head = old_head->next;
        if (q_head == NULL) {
            q_tail = NULL;
        } 
        free(old_head);
    }
}

void process_heap_object_recursive(void *heap_object)
{
    // TODO: essentially copy Stephens's code
}

void gc_init(void)
{
    // http://man7.org/linux/man-pages/man5/proc.5.html Bottom of stack: 28th value in /proc/self/stat
    FILE *process_stats_fp;
    process_stats_fp = fopen("/proc/self/stat", "r");
    assert(process_stats_fp != NULL);

    fscanf(process_stats_fp,
           "%*d %*s %*c %*d %*d %*d %*d %*d %*u "
           "%*u %*u %*u %*u %*u %*u %*d %*d "
           "%*d %*d %*d %*d %*u %*u %*d "
           "%*u %*u %*u %lu", (uintptr_t *) &stack_base);
    fclose(process_stats_fp);
    debug_print("Base of stack: %p\n", stack_base);
}

/*
gc_malloc memory block layout:

+-----------------------------+-----------+-----------+------------------+
|          User data          | *previous |   *next   | Stephen's Insert |
+-----------------------------+-----------+-----------+------------------+
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
    void *block = malloc(size);
    if (block == NULL) {
        debug_print("Malloc failure...\tSize request: %zu\n", size);
        return NULL;
    }
    PREVIOUS_POINTER(block) = &heap_list_head; // point previous pointer at address of heap_list_head
    NEXT_POINTER(block) = heap_list_head; // point next pointer at address of next block
    if (heap_list_head != NULL) { 
        PREVIOUS_POINTER(heap_list_head) = block; // point previous pointer of next block at new block
    }
    heap_list_head = block; // new block at beginning of linked list
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
        if (previous != &heap_list_head) {
            NEXT_POINTER(previous) = next;
        } else {
            heap_list_head = next;
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
    // request a new, larger block of memory.
    void *new = realloc(ptr, size);
    // if realloc failed, return NULL
    if (new == NULL) {
        debug_print("Realloc failure...\tPointer: %p\tSize request: %zu\n", ptr, size);
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
    if (previous != &heap_list_head) {
        NEXT_POINTER(previous) = new;
    } else {
        heap_list_head = new;
    }
    if (next != NULL) {
        PREVIOUS_POINTER(next) = new;
    }
    debug_print("Block resized, moved to new location...\tAddress: %p\tSize: %zu\tPrevious: %p\tNext: %p\n",
            new, malloc_usable_size(new), previous, next);
    return new;
}

void *pointed_to_heap_block(void *pointer)
{
    void *heap_block = heap_list_head;
    if (heap_block == NULL) {
        return NULL;
    }
    while (heap_block != NULL) {
        if (pointer >= heap_block && pointer < INSERT_ADDRESS(heap_block)) {
            return heap_block;
        }
        heap_block = NEXT_POINTER(heap_block);
    }
    return NULL;
}

void scan_stack_for_pointers_to_heap(void)
{
    void *heap_block = NULL;
    void *stack_pointer;
    asm volatile ("movq %%rsp, %0" : "=r" (stack_pointer)); // 'movq' and 'rsp' required for 64-bit
    void *stack_walker = stack_pointer;

    assert(stack_base != NULL);
    assert(stack_pointer != NULL);
    debug_print("Commencing stack scan...\tStack pointer: %p\tStack base: %p\n", stack_pointer, stack_base);

    for (stack_walker; stack_walker<stack_base; stack_walker+=POINTER_SIZE) {
        heap_block = pointed_to_heap_block(*((void **) stack_walker));
        if (heap_block != NULL) {
            debug_print("Found pointer to heap block %p\n", heap_block);
            enqueue_heap_queue_node(heap_block);
        }
    }
    debug_print("Stack scan complete\n");
}

void scan_data_segment_for_pointers_to_heap(void)
{
    debug_print("Commencing .data scan...\tdata_start: %p\tedata: %p\n", &data_start, &edata);
    void *heap_block = NULL;
    void *segment_walker = &data_start;

    for (segment_walker; segment_walker<(void *)&edata; segment_walker+=POINTER_SIZE) {
        debug_print(".data walker address: %p\tpointer value: %p\n", segment_walker, *((void **) segment_walker));
        heap_block = pointed_to_heap_block(*((void **) segment_walker));
        if (heap_block != NULL) {
            debug_print("Found pointer to heap block %p\n", heap_block);
            enqueue_heap_queue_node(heap_block);
        }
    }
    debug_print(".data scan complete\n");
}

void scan_bss_segment_for_pointers_to_heap(void)
{
    debug_print("Commencing .bss scan...\t__bss_start: %p\tend: %p\n", &__bss_start, &end);
    void *heap_block = NULL;
    void *segment_walker = &__bss_start;

    for (segment_walker; segment_walker<(void *)&end; segment_walker+=POINTER_SIZE) {
        debug_print(".bss walker address: %p\tpointer value: %p\n", segment_walker, *((void **) segment_walker));
        heap_block = pointed_to_heap_block(*((void **) segment_walker));
        if (heap_block != NULL) {
            if (segment_walker == &heap_list_head && heap_block == heap_list_head) {
                continue;
            }
            debug_print("Found pointer to heap block %p\n", heap_block);
            enqueue_heap_queue_node(heap_block);
        }
    }
    debug_print(".bss scan complete\n");
}

void sweep(void) 
{
    debug_print("Commencing sweep...\n");
    void *previous = &heap_list_head;
    void *current = heap_list_head;
    void *next;
    while (current != NULL) {
        if (marked(current)) {
            debug_print("Skipping marked block...\tAddress: %p\n", current);
            unmark_block(current);
            next = NEXT_POINTER(current);
            previous = current;
        } else {
            next = NEXT_POINTER(current);
            if (heap_list_head == current) {
                heap_list_head = next;
            } else {
                NEXT_POINTER(previous) = next;
            }
            debug_print("Freeing unmarked block...\tAddress: %p\n", current);
            if (next != NULL) {
                bool mark_present = marked(next); 
                PREVIOUS_POINTER(next) = previous;
                if (mark_present) {
                    mark_block(next);
                }
            }
            free(current);
        }
        current = next;
    }
    debug_print("Sweep complete\n");
}

void print_heap(void)
{
    void *current = heap_list_head;
    printf("Allocated blocks...\n");
    if (current == NULL) {
        printf("Heap empty\n");
    }
    while (current != NULL) {
        printf("Address: %p\tMarked: %d\n", current, marked(current));
        current = NEXT_POINTER(current);
    }
}