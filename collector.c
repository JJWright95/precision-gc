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