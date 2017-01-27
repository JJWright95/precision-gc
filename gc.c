#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>

#include "heap_index.h"
#include "gc.h"
#include "pointer_macros.h"
#include "liballocs.h"
#include "uniqtype.h"
#include "uniqtype-defs.h"


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

void dequeue_heap_queue_node(void)
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

#define IS_PLAUSIBLE_POINTER(p) (!(p) || ((p) == (void*) -1) || (((uintptr_t) (p)) >= 4194304 && ((uintptr_t) (p)) < 0x800000000000ul))

void process_heap_object_recursive(void *object, struct uniqtype *object_type)
{
    struct allocator *a;
	void *alloc_start;
	unsigned long alloc_size_bytes;
	struct uniqtype *alloc_uniqtype = object_type;
	void *alloc_site;

	if (alloc_uniqtype == NULL) {
		struct liballocs_err *err = __liballocs_get_alloc_info(object, 
			&a,
			&alloc_start,
			&alloc_size_bytes,
			&alloc_uniqtype,
			&alloc_site);
	
		assert(err == NULL);

		if (alloc_uniqtype->make_precise != NULL) {
			alloc_uniqtype = alloc_uniqtype->make_precise(alloc_uniqtype,
				NULL, 0,
				(void *) object, (void *) alloc_start, alloc_size_bytes, 
				__builtin_return_address(0),
				NULL);
		}
	}	
	
	if (alloc_uniqtype == &__uniqtype__void) return;
	if (alloc_uniqtype == NULL) return;
	if (!UNIQTYPE_HAS_SUBOBJECTS(alloc_uniqtype)) return;

	struct uniqtype_rel_info *related = &alloc_uniqtype->related[0];
	unsigned members;
	bool is_array;

	if (UNIQTYPE_IS_COMPOSITE_TYPE(alloc_uniqtype)) {
		is_array = false;
		members = UNIQTYPE_COMPOSITE_MEMBER_COUNT(alloc_uniqtype);
	} else {
		is_array = true;
		members = UNIQTYPE_ARRAY_LENGTH(alloc_uniqtype);
	}


	for (unsigned i=0; i<members; ++i, related += (is_array ? 0 : 1)) {
		// if we're an array, the element type should have known length (pos_maxoff)
		assert(!is_array || UNIQTYPE_HAS_KNOWN_LENGTH(related->un.memb.ptr));
		struct uniqtype *element_type = is_array ? UNIQTYPE_ARRAY_ELEMENT_TYPE(alloc_uniqtype) : 
			related->un.memb.ptr;
		long member_offset = is_array ? (i * UNIQTYPE_ARRAY_ELEMENT_TYPE(alloc_uniqtype)->pos_maxoff) 
			: related->un.memb.off;
		
		/* Is it a pointer? If so, add it to the heap object queue. */
		if (UNIQTYPE_IS_POINTER_TYPE(element_type)) {
			// get the address of the pointed-to object
			void *pointed_to_object = *(void**)((char*) object + member_offset);
			/* Check sanity of the pointer. We might be reading some union'd storage
			 * that is currently holding a non-pointer. */
            if ((pointed_to_object != NULL) && IS_PLAUSIBLE_POINTER(pointed_to_object)) {
				/* add node to queue */
				enqueue_heap_queue_node(pointed_to_object);
			} else if (!pointed_to_object || pointed_to_object == (void *) -1) {
				/* null pointer, do nothing */
			} else {
				debug_print("Warning: insane pointer value %p found in field index %d in object %p, type %s\n",
					pointed_to_object, i, object, NAME_FOR_UNIQTYPE(alloc_uniqtype));
			}
		} else if (UNIQTYPE_IS_COMPOSITE_TYPE(element_type)) { /* Else is it a thing with structure? If so, recurse. */
			void * subobject_address = (void *) ((char *) object + member_offset);
            process_heap_object_recursive(subobject_address, element_type); 
		}
	}
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
	memset(block+size, 0x0, INSERT_ADDRESS(block)-(block+size)); // zero out excess bytes in malloc bucket
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
    memset(new+size, 0x0, INSERT_ADDRESS(new)-(new+size)); // zero out excess bytes in malloc bucket
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
	debug_print("stack address: %p\tpointer value: %p\n", stack_walker, *((void **) stack_walker));
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

void mark_reachable_heap_objects(void)
{
    void *heap_object;
    while (q_head != NULL) {
        heap_object = q_head->heap_object;
        if (!marked(heap_object)) {
            process_heap_object_recursive(heap_object, NULL);
            mark_block(heap_object);
        }
		dequeue_heap_queue_node();
    }
}

void collect(void)
{
    assert(q_head == NULL);
    assert(q_tail == NULL);

    scan_stack_for_pointers_to_heap();
    scan_data_segment_for_pointers_to_heap();
    scan_bss_segment_for_pointers_to_heap();

    mark_reachable_heap_objects();

    sweep();
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
