#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>

#include "gc.h"

struct obj {
	int *i;
	double d;
	short s;
};

struct obj *objPtr;

void test(void)
{
	objPtr = gc_malloc(sizeof(struct obj));
	objPtr->i = gc_malloc(sizeof(int));
	*(objPtr->i) = 8;
}

int main(void)
{
	gc_init();

	// should not be collected
	struct obj *obj_ptr = gc_malloc(sizeof(struct obj));
	int *int_ptr = gc_malloc(sizeof(int));
	
	// should be collected
	gc_malloc(sizeof(double));
	gc_malloc(sizeof(long));

	test();

	print_heap();
	collect();

	print_heap();

	return 0;
}
