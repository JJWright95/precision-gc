#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>

#include "gc.h"

struct GC_prof_stats_s *stats;


int main(void)
{
	FILE *mem_count_fp = fopen("mem_count_boehm.csv", "w");
	GC_INIT();
	
	#define BUFF_SIZE 100
	#define OBJ_SIZE 256
	
	unsigned long *buffer = GC_MALLOC(BUFF_SIZE*sizeof(unsigned long));
	
	stats = GC_MALLOC(sizeof(struct GC_prof_stats_s));
	
	unsigned long total = 0;
	unsigned long old = 0;
	unsigned long current = 0;
	
	for (int i=0; i<BUFF_SIZE; i++) {
		buffer[i] = (unsigned long) GC_MALLOC(OBJ_SIZE*sizeof(unsigned long));
		current = GC_get_bytes_since_gc();
		if (current < old) {
			total += current;
			GC_get_prof_stats(stats, sizeof(struct GC_prof_stats_s));
			total -= stats->bytes_reclaimed_since_gc;
			printf("Bytes reclaimed since GC: %ld\n", stats->bytes_reclaimed_since_gc);
		} else {
			total += (current - old);
		}
		old = current;
		fprintf(mem_count_fp, "%ld\n", total);
		/*GC_get_prof_stats(stats, sizeof(struct GC_prof_stats_s));
		fprintf(mem_count_fp, "%ld\n", stats->heapsize_full - stats->free_bytes_full);*/
	}
	
	printf("Completed %d collections\n", GC_gc_no);

	fclose(mem_count_fp);
	return 0;
}
