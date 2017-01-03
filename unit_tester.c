#include <stdio.h>
#include <assert.h>
#include <malloc.h>
#include <stdint.h>
#include "collector.h"
#include "pointer_macros.h"

extern void *used_head;

int test_gc_malloc(void) 
{
    printf("Testing \'gc_malloc\'...\n");
    int chain_error = 0;
    printf("Testing linked-list pointer threading... ");
    void *block1 = gc_malloc(40);
    if (used_head != block1) {
        chain_error++;
    }
    if (PREVIOUS_POINTER(block1) != &used_head) {
        chain_error++;
    }
    if (NEXT_POINTER(block1) != NULL) {
        chain_error++;
    }
    void *block2 = gc_malloc(40);
    if (used_head != block2) {
        chain_error++;
    }
    if (PREVIOUS_POINTER(block2) != &used_head) {
        chain_error++;
    }
    if (NEXT_POINTER(block2) != block1) {
        chain_error++;
    }
    if (PREVIOUS_POINTER(block1) != block2) {
        chain_error++;
    }
    free(block1);
    free(block2);
    chain_error == 0 ? printf("passed.\n") : printf("failed.\n");
    printf("Testing with extreme input values... ");
    int input_error = 0;
    block1 = gc_malloc(0);
    if (block1 == NULL) { 
        input_error++;
    }
    free(block1);
    block1 = gc_malloc(-50);
    if (block1 != NULL) { 
        input_error++;
    }
    free(block1);
    input_error == 0 ? printf("passed.\n") : printf("failed.\n");
    if (chain_error+input_error == 0) {
        printf("Test complete: passed.\n\n");
    } else {
        printf("Test complete: failed.\n\n");
    }
    return chain_error+input_error;
}

int test_marked(void) 
{
    printf("Testing \'marked\'...\n");
    printf("Allocating block...\n");
    void *block = gc_malloc(16);
    printf("Verifying block unmarked...\n");
    bool block_unmarked = marked(block);
    printf("Marking block...\n");
    PREVIOUS_POINTER(block) = (void *) ((uintptr_t) PREVIOUS_POINTER(block) | 0x1);
    printf("Testing for presence of mark...\n");
    bool block_marked = marked(block);
    free(block);
    used_head = NULL;
    if (!block_unmarked && block_marked) {
        printf("Test complete: passed.\n\n");
        return 0;
    } else {
        printf("Test complete: failed.\n\n");
        return 1;
    }
}

int test_mark_block(void) 
{
    printf("Testing \'mark_block\'...\n");
    printf("Allocating block...\n");
    void *block = gc_malloc(16);
    assert(!marked(block));
    printf("Marking block...\n");
    mark_block(block);
    bool block_marked = marked(block);
    free(block);
    used_head = NULL;
    if (block_marked) {
        printf("Block marked successfully.\nTest complete: passed.\n\n");
        return 0;
    } else {
        printf("Failed to mark block.\nTest complete: failed.\n\n");
        return 1;
    }
}

int test_unmark_block(void) 
{
    printf("Testing \'unmark_block\'...\n");
    printf("Allocating block...\n");
    void *block = gc_malloc(16);
    assert(!marked(block));
    printf("Marking block...\n");
    mark_block(block);
    printf("Unmarking block...\n");
    unmark_block(block);
    bool block_marked = marked(block);
    free(block);
    used_head = NULL;
    if (!block_marked) {
        printf("Mark removed successfully.\nTest complete: passed.\n\n");
        return 0;
    } else {
        printf("Mark not removed.\nTest complete: failed.\n\n");
        return 1;
    }
}

int main(void) 
{
    int errors = 0;
    errors += test_gc_malloc();
    errors += test_marked();
    errors += test_mark_block();
    errors += test_unmark_block();
    if (errors == 0) {
        printf("All unit tests completed sucessfully.\n");
    } else {
        printf("Test(s) failed: \t%d\n", errors);
    }
    return 0;
}