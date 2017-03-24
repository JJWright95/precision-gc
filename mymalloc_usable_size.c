#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <assert.h>
#define RELF_DEFINE_STRUCTURES 1
#include "relf.h"

void *__mymalloc_lowest = (void*) -1;
void *__mymalloc_highest;

size_t mymalloc_usable_size(void* ptr);
size_t malloc_usable_size(void* ptr)
{
	static size_t (*orig_malloc_usable_size)(void*);
	if (!orig_malloc_usable_size) orig_malloc_usable_size = fake_dlsym(RTLD_NEXT, "malloc_usable_size");
	if (!orig_malloc_usable_size) abort();
	
	if ((uintptr_t) ptr <= 0x1000000) return orig_malloc_usable_size(ptr);
	else return mymalloc_usable_size(ptr);
}
size_t __mallochooks_malloc_usable_size(void* ptr) __attribute__((alias("malloc_usable_size")));
size_t __real_malloc_usable_size(void* ptr) __attribute__((alias("malloc_usable_size")));

void __real_myfree(void*);
void __wrap_myfree(void* ptr)
{
	__real_myfree(ptr);
}
