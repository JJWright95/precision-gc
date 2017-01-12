#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>

#include "liballocs.h"

struct obj {
	int i;
	double d;
	short s;
};

struct obj *objPtr;

int main(void)
{
	printf("Struct object size:\t%d\n", sizeof(struct obj));
	objPtr = malloc(sizeof(struct obj));
	assert(objPtr);
	printf("objPtr address:\t %p\n", objPtr);
	objPtr->i = 10;
	objPtr->d = 2.5;
	objPtr->s = 4;
	
	struct uniqtype *u = __liballocs_get_alloc_type(objPtr);

	return 0;
}
