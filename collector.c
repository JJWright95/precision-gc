#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <stdarg.h>

const bool DEBUG = false;

const int POINTER_SIZE = sizeof(void *);
static void *used_head = NULL;

printf_debug(const char *format, ...)
{
    if (DEBUG) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}

void mark_block(void *block) {
    *((long *) block) = *((long *) block) | 0x1L;
}

void unmark_block(void *block) {
    *((long *) block) = *((long *) block) & 0xfffffffffffffffe;
}

bool marked(void *block) {
    return *((long *) block) & 1L;
}

void *gc_malloc(size_t size) {
    // abort if size+2*POINTER_SIZE causes overflow
    assert(size < size+2*POINTER_SIZE);

    void *block = malloc(size+2*POINTER_SIZE);
    if (block == NULL) {
        printf_debug("Malloc failure...\t Size request: %ld", size+2*POINTER_SIZE);
        return NULL;
    }
    *((long *) block) = (long) &used_head; // point previous pointer at address of used_head
    *((long *) ((char *) block+POINTER_SIZE)) = (long) used_head; // point next pointer at address of next block
    if (used_head != NULL) { 
        *((long *) used_head) = (long) block; // point previous pointer of next block at new block
    }
    used_head = block; // new block at beginning of linked list
    printf_debug("Block allocated...\t Address: %p \t Size: %ld \t Previous: %p \t Next: %p \n", 
            (char *) block+2*POINTER_SIZE, size, (void *) *((long *) block), (void *) *((long *) ((char *) block+POINTER_SIZE)));
    return (char *) block+2*POINTER_SIZE;
}

void *gc_realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return gc_malloc(size);
    }
    void *current = (char *) ptr-2*POINTER_SIZE;
    void *previous = (void *) *((long *) current);
    void *next = (void *) *((long *) ((char *) current+POINTER_SIZE));
    // if size request is 0, free block
    if (size == 0) {
        printf_debug("Realloc size request 0, freeing block...\t Address: %p\n", ptr);
        if (next != NULL) {
            *((long *) next) = (long) previous;
        }
        if (used_head == current) {
                used_head = next;
        }
        *((long *) ((char *) previous+POINTER_SIZE)) = (long) next;
        free(current);
        return NULL;
    }
    // request a new, larger block of memory. Abort if size+2*POINTER_SIZE causes overflow
    assert(size < size+2*POINTER_SIZE);
    void *new = realloc(current, size+2*POINTER_SIZE);
    // if realloc failed, return NULL
    if (new == NULL) {
        printf_debug("Realloc failure...\t Pointer: %p\t Size request: %ld\n", current, size+2*POINTER_SIZE);
        return NULL;
    }
    // if data was not moved simply return old pointer
    if (current == new) {
        printf_debug("Block resized in place...\t Address: %p\t Size: %ld \t Previous: %p\t Next: %p\n",
                (char *) current+2*POINTER_SIZE, size, previous, next);
        return (char *) current+2*POINTER_SIZE;
    }
    // if get here, data has been moved. Update previous and next block pointers
    *((long *) ((char *) previous+POINTER_SIZE)) = (long) new;
    *((long *) next) = (long) new;
    printf_debug("Block resized, moved to new location...\t Address: %p\t Size: %ld\t Previous: %p\t Next: %p\n",
            (char *) new+2*POINTER_SIZE, size, previous, next);
    return (void *) ((char *) new+2*POINTER_SIZE);
}

void sweep(void) {
    printf_debug("Commencing sweep...\n");
    void *previous = used_head;
    void *current = used_head;
    void *next;
    while (current != NULL && current != (void *) 1L) {
        if (marked(current)) {
            printf_debug("Skipping marked block...\t Address: %p\n", (char *) current+2*POINTER_SIZE);
            unmark_block(current);
            next = (void *) *((long *) ((char *) current+POINTER_SIZE));
            previous = current;
        } else {
            next = (void *) *((long *) ((char *) current+POINTER_SIZE));
            if (used_head == current) {
                used_head = next;
            }
            printf_debug("Freeing unmarked block...\t Address: %p\n", (char *) current+2*POINTER_SIZE);
            if (next != NULL && next != (void *) 1L) {
                bool markPresent = marked(next); 
                *((long *) next) = (long) previous;
                if (markPresent) {
                    mark_block(next);
                }
                *((long *) ((char *) previous+POINTER_SIZE)) = (long) next;
            }
            free(current);
        }
        current = next;
    } 
}

void printHeap(void) {
    void *current = used_head;
    printf("Allocated blocks...\n");
    if (current == NULL) {
        printf("Heap empty\n");
    }
    while (current != NULL) {
        printf("Address: %p\t Marked: %d\n", (char *) current+2*POINTER_SIZE, marked(current));
        current = (void *) *((long *) ((char *) current+POINTER_SIZE));
        //printf("Next block address: %p \n", current);
    }
}