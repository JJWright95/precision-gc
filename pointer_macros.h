#ifndef POINTER_MACROS_H
#define POINTER_MACROS_H

/*
gc_malloc memory block layout:

+-----------------------------+-----------+-----------+
|          User data          | *previous |   *next   |
+-----------------------------+-----------+-----------+
*/

#define POINTER_SIZE sizeof(void *)
#define INSERT_ADDRESS(BLOCK_ADDRESS) ((void *) ((char *) BLOCK_ADDRESS+malloc_usable_size(BLOCK_ADDRESS)-2*POINTER_SIZE))
#define PREVIOUS_POINTER(BLOCK_ADDRESS) (*((void **) ((char *) BLOCK_ADDRESS+malloc_usable_size(BLOCK_ADDRESS)-2*POINTER_SIZE)))
#define NEXT_POINTER(BLOCK_ADDRESS) (*((void **) ((char *) BLOCK_ADDRESS+malloc_usable_size(BLOCK_ADDRESS)-POINTER_SIZE)))

/* What macros will look like when using Stephen's insert

#define PREVIOUS_POINTER(BLOCK_ADDRESS) (*((void **) ((char *) __liballocs_insert_for_chunk_and_usable_size(BLOCK_ADDRESS, malloc_usable_size(BLOCK_ADDRESS)) -2*POINTER_SIZE)))
#define NEXT_POINTER(BLOCK_ADDRESS) (*((void **) ((char *) __liballocs_insert_for_chunk_and_usable_size(BLOCK_ADDRESS, malloc_usable_size(BLOCK_ADDRESS)) -POINTER_SIZE)))
*/

/*
gc_malloc memory block layout:

+-----------------------------+-----------+-----------+------------------+
|          User data          | *previous |   *next   | Stephen's Insert |
+-----------------------------+-----------+-----------+------------------+
*/

#endif