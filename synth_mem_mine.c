#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>

#include "gc.h"

extern FILE *mem_count_fp;

int main(void)
{
	mem_count_fp = fopen("mem_count_mine.csv", "w");
	GC_INIT();
	
	#define BUFF_SIZE 100
	#define OBJ_SIZE 256
	
	unsigned long *buffer = GC_MALLOC(BUFF_SIZE*sizeof(unsigned long));
	
	for (int i=0; i<BUFF_SIZE; i++) {
		buffer[i] = (unsigned long) GC_MALLOC(OBJ_SIZE*sizeof(unsigned long));
	}

	printf("Completed %d collections\n", GC_num_collections());

	fclose(mem_count_fp);
	return 0;
}
