#include <stdio.h>
#include <stdlib.h>

static void *usedHead = NULL;

void *gc_malloc(size_t size) {
    void *addr = malloc(size+2*sizeof(void*));
    if (addr == NULL) {
        printf("Malloc failure...\t Size request: %ld", size+2*sizeof(void*));
        return NULL;
    }
    *((long *) addr) = (long) &usedHead; // point previous pointer at address of usedHead
    *((long *) ((char *) addr+sizeof(void*))) = (long) usedHead; // point next pointer at address of next block
    if (usedHead != NULL) { 
        *((long *) usedHead) = (long) addr; // point previous pointer of next block at new block
    }
    usedHead = addr; // new block at beginning of linked list
    printf("Block allocated...\t Address: %p \t Size: %ld \t Previous: %p \t Next: %p \n", 
            (char *) addr+2*sizeof(void*), size, (void *) *((long *) addr), (void *) *((long *) ((char *) addr+sizeof(void*))));
    return (char *) addr+2*sizeof(void*);
}

void mark(void *block) {
    *((long *) block) = *((long *) block) | 0x1L;
}

void unmark(void *block) {
    *((long *) block) = *((long *) block) & 0xfffffffffffffffe;
}

int marked(void *block) {
    return *((long *)block) & 1L;
}

void sweep (void) {
    printf("Commencing sweep...\n");
    void *previous = usedHead;
    void *current = usedHead;
    void *next;
    while (current != NULL && current != (void *) 1L) {
        if (marked(current)) {
            printf("Skipping marked block... \t Address: %p\n", current+sizeof(void*));
            unmark(current);
            next = (void *) *((long *) current);
            previous = current;
        } else {
            next = (void *) *((long *) current);
            if (usedHead == current) {
                usedHead = next;
            }
            printf("Freeing unmarked block... \t Address: %p\n", current+sizeof(void*));
            free(current);
            *((long *) previous) = (long) next;
        }
        current = next;
    } 
}

void printHeap(void) {
    void *current = usedHead;
    printf("Allocated blocks...\n");
    if (current == NULL) {
        printf("Heap empty\n");
    }
    while (current != NULL) {
        printf("Address: %p \t Marked: %d\n", current+sizeof(void*), marked(current));
        if (marked(current)) {
            current = (void *) (*((long *) current) & 0xfffffffffffffffe);
        } else {
            current = (void *) *((long *) current);
        }
    }
}