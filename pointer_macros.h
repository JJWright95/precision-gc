#ifndef POINTER_MACROS_H
#define POINTER_MACROS_H

/*
gc_malloc memory block layout:

+-----------------------------+-----------+-----------+------------------+
|          User data          | *previous |   *next   | Stephen's Insert |
+-----------------------------+-----------+-----------+------------------+
                              ^
                        INSERT_ADDRESS
*/

#define POINTER_SIZE sizeof(void *)
#define INSERT_ADDRESS(BLOCK_ADDRESS) ((void *) ((char *) BLOCK_ADDRESS+malloc_usable_size(BLOCK_ADDRESS)-sizeof(struct insert)-2*POINTER_SIZE))
#define PREVIOUS_POINTER(BLOCK_ADDRESS) (*((void **) ((char *) BLOCK_ADDRESS+malloc_usable_size(BLOCK_ADDRESS)-sizeof(struct insert)- 2*POINTER_SIZE)))
#define NEXT_POINTER(BLOCK_ADDRESS) (*((void **) ((char *) BLOCK_ADDRESS+malloc_usable_size(BLOCK_ADDRESS)-sizeof(struct insert)-POINTER_SIZE)))


#endif
