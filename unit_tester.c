#include <stdio.h>
#include <assert.h>
#include <malloc.h>
#include <stdint.h>

#include "gc.h"
#include "pointer_macros.h"


extern void *heap_list_head;

int test_gc_malloc_case_1(void)
{
    assert(heap_list_head == NULL);
    printf("Testing linked-list pointer threading... ");
    int error = 0;
    void *block1 = gc_malloc(40);
    if (heap_list_head != block1) {
        error++;
    }
    if (PREVIOUS_POINTER(block1) != &heap_list_head) {
        error++;
    }
    if (NEXT_POINTER(block1) != NULL) {
        error++;
    }
    void *block2 = gc_malloc(40);
    if (heap_list_head != block2) {
        error++;
    }
    if (PREVIOUS_POINTER(block2) != &heap_list_head) {
        error++;
    }
    if (NEXT_POINTER(block2) != block1) {
        error++;
    }
    if (PREVIOUS_POINTER(block1) != block2) {
        error++;
    }
    free(block1);
    free(block2);
    heap_list_head = NULL;
    error == 0 ? printf("passed.\n") : printf("failed.\n");
    return error;
}

int test_gc_malloc_case_2(void)
{
    assert(heap_list_head == NULL);
    printf("Testing with extreme input values... ");
    int error = 0;
    void *block = gc_malloc(0);
    if (block == NULL) { 
        error++;
    }
    free(block);
    heap_list_head = NULL;
    block = gc_malloc(-50);
    if (block != NULL) { 
        error++;
    }
    error == 0 ? printf("passed.\n") : printf("failed.\n");
    return error;
}

int test_gc_malloc(void) 
{
    printf("Testing \'gc_malloc\'...\n");
    int errors = 0;
    errors += test_gc_malloc_case_1();
    errors += test_gc_malloc_case_2();
    if (errors == 0) {
        printf("Test complete: passed.\n\n");
    } else {
        printf("Test complete: failed.\n\n");
    }
    return errors;
}

int test_marked(void) 
{
    assert(heap_list_head == NULL);
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
    heap_list_head = NULL;
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
    assert(heap_list_head == NULL);
    printf("Testing \'mark_block\'...\n");
    printf("Allocating block...\n");
    void *block = gc_malloc(16);
    assert(!marked(block));
    printf("Marking block...\n");
    mark_block(block);
    bool block_marked = marked(block);
    free(block);
    heap_list_head = NULL;
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
    assert(heap_list_head == NULL);
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
    heap_list_head = NULL;
    if (!block_marked) {
        printf("Mark removed successfully.\nTest complete: passed.\n\n");
        return 0;
    } else {
        printf("Mark not removed.\nTest complete: failed.\n\n");
        return 1;
    }
}

int test_gc_realloc_case_1(void)
{
    assert(heap_list_head == NULL);
    printf("Testing 'free' on size request 0... ");
    int error = 0;
    void *block1 = gc_malloc(40);
    void *block2 = gc_malloc(40);
    void *realloc1 = gc_realloc(block1, 0);
    if (NEXT_POINTER(block2) != NULL) {
        error++;
    }
    if (realloc1 != NULL) {
        error++;
    }
    void *realloc2 = gc_realloc(block2, 0);
    if (realloc2 != NULL) {
        error++;
    }
    if (heap_list_head != NULL) {
        error++;
    }
    error == 0 ? printf("passed.\n") : printf("failed.\n");
    return error;
}

int test_gc_realloc_case_2(void)
{
    assert(heap_list_head == NULL);
    printf("Testing handling of unsatisfiable size request... ");
    int error = 0;
    void *block1 = gc_malloc(20);
    void *realloc1 = gc_realloc(block1, 0xfffffffffffff);
    if(realloc1 != NULL || block1 == NULL) {
        error++;
    }
    free(block1);
    heap_list_head = NULL;
    error == 0 ? printf("passed.\n") : printf("failed.\n");
    return error;
}

int test_gc_realloc_case_3(void)
{
    assert(heap_list_head == NULL);
    printf("Testing block resizing and relocation... ");
    int error = 0;
    void *block1 = gc_malloc(20);
    void *block2 = gc_malloc(20);
    void *realloc1 = gc_realloc(block1, 1200);
    if (realloc1 != block1) {
        if (PREVIOUS_POINTER(realloc1) != block2 || NEXT_POINTER(realloc1) != NULL) {
            error++;
        }
    } else {
        error++;
    }
    free(block2);
    free(realloc1);
    heap_list_head = NULL;
    error == 0 ? printf("passed.\n") : printf("failed.\n");
    return error;
}

int test_gc_realloc_case_4(void)
{
    assert(heap_list_head == NULL);
    printf("Testing in-place block resizing... ");
    int error = 0;
    void *block1 = gc_malloc(20);
    void *realloc1 = gc_realloc(block1, 24);
    if (realloc1 == block1) {
        if (PREVIOUS_POINTER(realloc1) != &heap_list_head || NEXT_POINTER(realloc1) != NULL) {
            error++;
        }
    } else {
        error+=4;
    }
    free(realloc1);
    heap_list_head = NULL;
    error == 0 ? printf("passed.\n") : printf("failed.\n");
    return error;
}

int test_gc_realloc(void)
{
    printf("Testing \'gc_realloc\'...\n");
    int errors = 0;
    errors += test_gc_realloc_case_1();
    errors += test_gc_realloc_case_2();
    errors += test_gc_realloc_case_3();
    errors += test_gc_realloc_case_4();
    if (errors == 0) {
        printf("Test complete: passed.\n\n");
    } else {
        printf("Test complete: failed.\n\n");
    }
    return errors;
}

int test_sweep_case_1(void)
{
    assert(heap_list_head == NULL);
    printf("Testing with empty heap... ");
    int error = 0;
    sweep();
    if (heap_list_head != NULL) {
        error++;
    }
    error == 0 ? printf("passed.\n") : printf("failed.\n");
    return error;
}

int test_sweep_case_2(void)
{
    assert(heap_list_head == NULL);
    printf("Testing with all blocks unmarked... ");
    int error = 0;
    gc_malloc(20);
    gc_malloc(20);
    sweep();
    if (heap_list_head != NULL) {
        error++;
    }
    error == 0 ? printf("passed.\n") : printf("failed.\n");
    return error;
}

int test_sweep_case_3(void)
{
    assert(heap_list_head == NULL);
    printf("Testing with all blocks marked... ");
    int error = 0;
    void *block1 = gc_malloc(20);
    void *block2 = gc_malloc(20);
    mark_block(block1);
    mark_block(block2);
    sweep();
    if (marked(block1) || marked(block2)) { 
        error++;
    }
    sweep();
    error == 0 ? printf("passed.\n") : printf("failed.\n");
    return error;
}

int test_sweep_case_4(void)
{
    assert(heap_list_head == NULL);
    printf("Testing with mixture of marked and unmarked blocks... ");
    int error = 0;
    void *block1 = gc_malloc(20);
    void *block2 = gc_malloc(20);
    mark_block(block1);
    sweep();
    if (heap_list_head != block1 || marked(block1) 
        || PREVIOUS_POINTER(block1) != &heap_list_head) {
        error++;
    }
    sweep();
    // test with sequence: marked, unmarked, marked
    block1 = gc_malloc(20);
    block2 = gc_malloc(20);
    void *block3 = gc_malloc(20);
    mark_block(block1);
    mark_block(block3);
    sweep();
    if (heap_list_head != block3 || marked(block1) || marked(block3) 
        || NEXT_POINTER(block3)!=block1 || PREVIOUS_POINTER(block1)!=block3) {
        error++;
    }
    error == 0 ? printf("passed.\n") : printf("failed.\n");
    return error;
}

int test_sweep()
{
    printf("Testing \'sweep\'...\n");
    int errors = 0;
    errors += test_sweep_case_1();
    errors += test_sweep_case_2();
    errors += test_sweep_case_3();
    errors += test_sweep_case_4();
    if (errors == 0) {
        printf("Test complete: passed.\n\n");
    } else {
        printf("Test complete: failed.\n\n");
    }
    return errors;
}

int main(void) 
{
    int errors = 0;
    errors += test_gc_malloc();
    errors += test_marked();
    errors += test_mark_block();
    errors += test_unmark_block();
    errors += test_gc_realloc();
    errors += test_sweep();
    if (errors == 0) {
        printf("All unit tests completed sucessfully.\n");
    } else {
        printf("Test(s) failed: \t%d\n", errors);
    }
    return 0;
}