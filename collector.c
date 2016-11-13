static void *usedHead = NULL;

void* gc_malloc(size_t numBytes) {
    void* addr = malloc(numBytes+sizeof(void*));
    *((long *) addr) = (long) usedHead;
    usedHead = addr; 
    printf("Block allocated...\t Address: %p \t Size: %ld \n", (void *) addr+sizeof(void *), numBytes);
    return addr+sizeof(void *);
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