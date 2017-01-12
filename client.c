#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>

#include "gc.h"
#include "liballocs.h"

struct obj {
	int i;
	double d;
	short s;
};

struct obj *objPtr;

int main(void)
{
	gc_init();
	printf("Struct object size:\t%d\n", sizeof(struct obj));
	objPtr = gc_malloc(sizeof(struct obj));

	void *ptr1 = gc_malloc(40);
	void *ptr2 = gc_malloc(80);
	void *ptr3 = gc_malloc(120);
	void *ptr4 = gc_malloc(160);

	assert(objPtr);
	printf("objPtr address:\t %p\n", objPtr);
	objPtr->i = 10;
	objPtr->d = 2.5;
	objPtr->s = 4;
	
	struct uniqtype *u = __liballocs_get_alloc_type(objPtr);

	print_heap();
	sweep();
	print_heap();

	return 0;
}
