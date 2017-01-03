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



int main(void) 
{
    int errors = 0;
    errors += test_gc_malloc();
    if (errors == 0) {
        printf("All unit tests completed sucessfully.\n");
    } else {
        printf("Test(s) failed: \t%d\n", errors);
    }
    return 0;
}