#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <semaphore.h>

#include "heap_index.h"
#include "gc.h"
#include "dlmalloc.h"
void *mymalloc(size_t);
void *myrealloc(void*, size_t);
void myfree(void*);
size_t mymalloc_usable_size(void*);

#include "liballocs.h"
#include "uniqtype.h"
#include "uniqtype-defs.h"
#include "pageindex.h"
#include "vas.h"


#ifdef DEBUG
#define DEBUG_TEST 1
#else
#define DEBUG_TEST 0
#endif

#define debug_print(...) do { if (DEBUG_TEST) fprintf(stderr, ##__VA_ARGS__); } while (0)

extern long get_heap_size(void);
static int num_collections = 0; 
extern bool GC_running;
bool GC_initialised = false;

/* semaphore to enforce exclusive access to gc_malloc allocator. */
sem_t allocator_mutex;

/* Private liballocs allocator for GC helper structures. */
extern void *__private_malloc(size_t size);
extern void __private_free(void *ptr);

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
    struct q_node *node = __private_malloc(sizeof(struct q_node));
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
        __private_free(old_head);
    }
}

// bigalloc linked-list data
struct bigalloc_node {
    unsigned long bigalloc_index;
    struct bigalloc_node *next;
};

struct bigalloc_node *bigallocs_list = NULL;

void new_bigalloc(void *pointer)
{
    if (!valid_bigalloc(pointer)) {
    	struct bigalloc_node *node = __private_malloc(sizeof(struct bigalloc_node));
    	node->bigalloc_index = pageindex[PAGENUM(pointer)];
    	node->next = bigallocs_list;
    	bigallocs_list = node;
    }
}

#define IS_PLAUSIBLE_POINTER(p) (((uintptr_t) (p)) >= 4194304 && ((uintptr_t) (p)) < 0x800000000000ul)

bool valid_bigalloc(void *pointer)
{
	unsigned long index = pageindex[PAGENUM(pointer)];
	struct bigalloc_node *b = bigallocs_list;
	while (b != NULL) {
		if (index == b->bigalloc_index) {
			return true;
		}
		b = b->next;
	}
	return false;
}



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
            if (valid_heap_object(pointed_to_object)) {
                /* add node to queue */
				enqueue_heap_queue_node(pointed_to_object);
				debug_print("Found heap object:\t%p\n", pointed_to_object);
			} else if (!pointed_to_object || pointed_to_object == (void *) -1) {
				/* null pointer, do nothing */
			} else {
				debug_print("Warning: insane pointer value %p found in field index %d in object %p, type %s\n",
					pointed_to_object, i, object, NAME_FOR_UNIQTYPE(alloc_uniqtype));
			}
		} else if (UNIQTYPE_IS_COMPOSITE_TYPE(element_type)) { /* Else is it a thing with structure? If so, recurse. */
			void *subobject_address = (void *) ((char *) object + member_offset);
            process_heap_object_recursive(subobject_address, element_type);
		}
	}
}

void gc_init(void)
{
    /* Number of fields preceding startstack field in /proc/self/stat */
    #define STAT_SKIP 27   

    /* Read the stack base value from /proc/self/stat using direct */
    /* I/O system calls in order to avoid calling malloc. */
    #define STAT_BUF_SIZE 4096

    char stat_buf[STAT_BUF_SIZE];
    int process_stats_fd;
    int i;
    int buf_offset = 0;
    int file_length;

    process_stats_fd = open("/proc/self/stat", O_RDONLY);
    assert(process_stats_fd >= 0);
    file_length = read(process_stats_fd, stat_buf, STAT_BUF_SIZE);
    close(process_stats_fd);

    /* Skip the required number of fields. */
    for (i=0; i<STAT_SKIP; ++i) {
      while (buf_offset < file_length && isspace(stat_buf[buf_offset++])) {
        /* empty */
      }
      while (buf_offset < file_length && !isspace(stat_buf[buf_offset++])) {
        /* empty */
      }
    }
    /* Skip spaces. */
    while (buf_offset < file_length && isspace(stat_buf[buf_offset])) {
      buf_offset++;
    }
    /* Find the end of the number and cut the buffer there. */
    for (i=0; buf_offset+i < file_length; i++) {
      if (!isdigit(stat_buf[buf_offset+i])) break;
    }
    assert(buf_offset+i < file_length);
    stat_buf[buf_offset+i] = '\0';

    /* Convert address string to base 10 digits. */
    stack_base = (void *) strtoull(&stat_buf[buf_offset], NULL, 10);
    assert((uintptr_t) stack_base > 0x100000 && ((uintptr_t) stack_base & (sizeof(void *)-1)) == 0);
    
    /* Initialise semaphore for exclusive access to allocator gc_malloc*/
    int err = sem_init(&allocator_mutex, 0, 1);
    assert(err == 0);
    GC_initialised = true;
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

extern void *__mymalloc_lowest, *__mymalloc_highest;
void *gc_malloc(size_t size)
{
	int err = sem_wait(&allocator_mutex);
	assert(err == 0);
    void *block = mymalloc(size);
    if (block == NULL) {
        debug_print("Malloc failure...\tSize request: %zu\n", size);
        return NULL;
    }
	if ((char*) block < __mymalloc_lowest) __mymalloc_lowest = block;
	if ((char*) block > __mymalloc_highest) __mymalloc_highest = block;
    memset(block, 0x0, INSERT_ADDRESS(block)-block); // zero out all bytes to prevent uninitialised address ghosting
    PREVIOUS_POINTER(block) = &heap_list_head; // point previous pointer at address of heap_list_head
    NEXT_POINTER(block) = heap_list_head; // point next pointer at address of next block
    if (heap_list_head != NULL) { 
        PREVIOUS_POINTER(heap_list_head) = block; // point previous pointer of next block at new block
    }
    heap_list_head = block; // new block at beginning of linked list
    debug_print("Block allocated...\tAddress:%p\tSize: %zu\tPrevious: %p\tNext: %p\n", 
    		block, mymalloc_usable_size(block), PREVIOUS_POINTER(block), NEXT_POINTER(block));
    
    /* Update bigallocation list */
	new_bigalloc(block);		
   	
   	err = sem_post(&allocator_mutex);
   	assert(err == 0);
    return block;
}

void *gc_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
        return gc_malloc(size);
    }
    
    int err = sem_wait(&allocator_mutex);
	assert(err == 0);
	
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
        myfree(ptr);
        err = sem_post(&allocator_mutex);
   		assert(err == 0);
        return NULL;
    }
    // clear previous and next pointers in original, small block
    PREVIOUS_POINTER(ptr) = (void *) 0;
    NEXT_POINTER(ptr) = (void *) 0;
    // request a new, larger block of memory.
    void *new = myrealloc(ptr, size);
    // if realloc failed, return NULL
    if (new == NULL) {
        debug_print("Realloc failure...\tPointer: %p\tSize request: %zu\n", ptr, size);
        PREVIOUS_POINTER(ptr) = previous;
        NEXT_POINTER(ptr) = next;
        err = sem_post(&allocator_mutex);
   		assert(err == 0);
        return NULL;
    }
    memset(new, 0x0, INSERT_ADDRESS(new)-new); // zero out all bytes to prevent uninitialised address ghosting
    // set previous and next block pointers at end of larger block
    PREVIOUS_POINTER(new) = previous;
    NEXT_POINTER(new) = next;
    // if data was not moved simply return old pointer
    if (ptr == new) {
        debug_print("Block resized in place...\tAddress: %p\tSize: %zu\tPrevious: %p\tNext: %p\n",
                ptr, mymalloc_usable_size(new), previous, next);
        err = sem_post(&allocator_mutex);
   		assert(err == 0);
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
            new, mymalloc_usable_size(new), previous, next);
            
    /* Update bigallocation list */
	new_bigalloc(new); 
	
    err = sem_post(&allocator_mutex);
   	assert(err == 0); 
    return new;
}

bool valid_heap_object(void *pointer)
{
	if (IS_PLAUSIBLE_POINTER(pointer)) {
		if (valid_bigalloc(pointer)) {

        	struct allocator *a = NULL;
	    	void *alloc_start = NULL;
	    	unsigned long alloc_size_bytes = 0;
	    	struct uniqtype *alloc_uniqtype = NULL;
	    	void *alloc_site = NULL;

			__liballocs_get_alloc_info(pointer, // Don't care about error return type 
				&a,
				&alloc_start,
				&alloc_size_bytes,
				&alloc_uniqtype,
				&alloc_site);
        
        	/* Valid mymalloc pointer if allocator is generic_malloc and block has non-zero size */
        	/* Only care about pointers to start of chunk, user must have valid reference to alloc_start */
        	if (a == &__generic_malloc_allocator 
            	&& alloc_size_bytes != 0 
            	&& pointer == alloc_start) {
            	return true;
        	}
        }           
    }
    return false;
}

void scan_stack_for_pointers_to_heap(void)
{
    void *stack_pointer;
    asm volatile ("movq %%rsp, %0" : "=r" (stack_pointer)); // 'movq' and 'rsp' required for 64-bit
    void *stack_walker = stack_pointer;

    assert(stack_base != NULL);
    assert(stack_pointer != NULL);
    debug_print("Commencing stack scan...\tStack pointer: %p\tStack base: %p\n", stack_pointer, stack_base);

    for (stack_walker; stack_walker<stack_base; stack_walker+=POINTER_SIZE) {
		debug_print("stack address: %p\tpointer value: %p\n", stack_walker, *((void **) stack_walker));
        if (valid_heap_object(*((void **) stack_walker))) {
            debug_print("Found pointer to heap block %p\n", *((void **) stack_walker));
            enqueue_heap_queue_node(*((void **) stack_walker));
        }
    }
    debug_print("Stack scan complete\n");
}

void scan_data_segment_for_pointers_to_heap(void)
{
    debug_print("Commencing .data scan...\tdata_start: %p\tedata: %p\n", &data_start, &edata);
    void *segment_walker = &data_start;

    for (segment_walker; segment_walker<(void *)&edata; segment_walker+=POINTER_SIZE) {
        debug_print(".data walker address: %p\tpointer value: %p\n", segment_walker, *((void **) segment_walker));
        if (valid_heap_object(*((void **) segment_walker))) {
            debug_print("Found pointer to heap block %p\n", *((void **) segment_walker));
            enqueue_heap_queue_node(*((void **) segment_walker));
        }
    }
    debug_print(".data scan complete\n");
}

void scan_bss_segment_for_pointers_to_heap(void)
{
    debug_print("Commencing .bss scan...\t__bss_start: %p\tend: %p\n", &__bss_start, &end);
    void *segment_walker = &__bss_start;

    for (segment_walker; segment_walker<(void *)&end; segment_walker+=POINTER_SIZE) {
        debug_print(".bss walker address: %p\tpointer value: %p\n", segment_walker, *((void **) segment_walker));
        if (valid_heap_object(*((void **) segment_walker))) {
            if (segment_walker == &heap_list_head && *((void **) segment_walker) == heap_list_head) {
                continue;
            }
            debug_print("Found pointer to heap block %p\n", *((void **) segment_walker));
            enqueue_heap_queue_node(*((void **) segment_walker));
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
            myfree(current);
        }
        current = next;
    }
    debug_print("Sweep complete\n");
}

void mark_reachable_heap_objects(void)
{
	debug_print("Commencing heap object traversal from root set, marking reachable objects...\n");
    void *heap_object;
    while (q_head != NULL) {
        heap_object = q_head->heap_object;
        // 0xccccccccccccccccc is sentinel value if not a GC'd object
        if (!marked(heap_object) && PREVIOUS_POINTER(heap_object) != 0xcccccccccccccccc) {
        	debug_print("Searching heap object %p for pointers to the heap...\n", heap_object);
            process_heap_object_recursive(heap_object, NULL);
            mark_block(heap_object);
        }
		dequeue_heap_queue_node();
    }
    debug_print("Heap object traversal from root set complete.\n");
}

void gc_collect(void)
{
    GC_running = true;
    num_collections++;
	debug_print("Commencing garbage collection...\n");
    assert(q_head == NULL);
    assert(q_tail == NULL);

    scan_stack_for_pointers_to_heap();
    scan_data_segment_for_pointers_to_heap();
    scan_bss_segment_for_pointers_to_heap();

    mark_reachable_heap_objects();

    sweep();
    debug_print("Garbage collection complete.\n");
    GC_running = false;
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

int GC_num_collections(void)
{
	return num_collections;
}

long GC_heap_size(void)
{
	return get_heap_size();
}
