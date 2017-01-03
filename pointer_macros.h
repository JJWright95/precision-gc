#ifndef __POINTER_MACROS_H__
#define __POINTER_MACROS_H__

#define POINTER_SIZE sizeof(void *)

#define PREVIOUS_POINTER(BLOCK_ADDRESS) (*((void **) ((char *) BLOCK_ADDRESS+malloc_usable_size(BLOCK_ADDRESS)-2*POINTER_SIZE)))
#define NEXT_POINTER(BLOCK_ADDRESS) (*((void **) ((char *) BLOCK_ADDRESS+malloc_usable_size(BLOCK_ADDRESS)-POINTER_SIZE)))

#endif