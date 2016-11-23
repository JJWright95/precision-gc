#include <stdio.h>
#include <stdlib.h>

const int PTR_SIZE = sizeof(void *);
static void *usedHead = NULL;

void markBlock(void *block) {
    *((long *) block) = *((long *) block) | 0x1L;
}

void unmarkBlock(void *block) {
    *((long *) block) = *((long *) block) & 0xfffffffffffffffe;
}

int marked(void *block) {
    return *((long *) block) & 1L;
}

void *gc_malloc(size_t size) {
    void *addr = malloc(size+2*PTR_SIZE);
    if (addr == NULL) {
        printf("Malloc failure...\t Size request: %ld", size+2*PTR_SIZE);
        return NULL;
    }
    *((long *) addr) = (long) &usedHead; // point previous pointer at address of usedHead
    *((long *) ((char *) addr+PTR_SIZE)) = (long) usedHead; // point next pointer at address of next block
    if (usedHead != NULL) { 
        *((long *) usedHead) = (long) addr; // point previous pointer of next block at new block
    }
    usedHead = addr; // new block at beginning of linked list
    printf("Block allocated...\t Address: %p \t Size: %ld \t Previous: %p \t Next: %p \n", 
            (char *) addr+2*PTR_SIZE, size, (void *) *((long *) addr), (void *) *((long *) ((char *) addr+PTR_SIZE)));
    return (char *) addr+2*PTR_SIZE;
}

void sweep(void) {
    printf("Commencing sweep...\n");
    void *previous = usedHead;
    void *current = usedHead;
    void *next;
    while (current != NULL && current != (void *) 1L) {
        if (marked(current)) {
            printf("Skipping marked block...\t Address: %p\n", (char *) current+2*PTR_SIZE);
            unmarkBlock(current);
            next = (void *) *((long *) ((char *) current+PTR_SIZE));
            previous = current;
        } else {
            next = (void *) *((long *) ((char *) current+PTR_SIZE));
            if (usedHead == current) {
                usedHead = next;
            }
            printf("Freeing unmarked block...\t Address: %p\n", (char *) current+2*PTR_SIZE);
            if (next != NULL && next != (void *) 1L) {
                int markPresent = marked(next); 
                *((long *) next) = (long) previous;
                if (markPresent) {
                    markBlock(next);
                }
                *((long *) ((char *) previous+PTR_SIZE)) = (long) next;
            }
            free(current);
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
        printf("Address: %p\t Marked: %d\n", (char *) current+2*PTR_SIZE, marked(current));
        current = (void *) *((long *) ((char *) current+PTR_SIZE));
        //printf("Next block address: %p \n", current);
    }
}