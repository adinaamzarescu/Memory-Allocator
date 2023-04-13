# Memory Allocator
#### Copyright Adina-Maria Amzarescu
_________________________________________________________________________

A minimalistic memory allocator that can be used to 
manually manage virtual memory. The goal is to have a 
reliable library that accounts for explicit allocation, 
reallocation, and initialization of memory.

_________________________________________________________________________

There are 4 main functions:

* os_malloc()

* os_calloc()

* os_realloc()

* os_free()

For a cleaner code I added 5 more functions:

* get_block_ptr()

* os_split_block()

* setup_data_block()

* coalesce_prev()

* coalesce_next()

_________________________________________________________________________

Each function in the program works with the block_meta 
structure, which represents a memory block and is stored 
as a linked list.

`os_malloc()`

The os_malloc function is responsible for allocating a block of memory 
of a certain size. It first checks if the requested size is zero, and 
if so, returns NULL. If this is the first allocation, it sets up the 
data_block. Otherwise, it tries to find a free block of memory that is 
large enough to fulfill the request. If a free block is found, it 
updates its metadata, coalesces it with any adjacent free blocks, zeros 
out the memory, and returns a pointer to the allocated memory. If no 
free block is found, it allocates a new block of memory and adds it to 
the end of the linked list. Finally, it returns a pointer to the 
allocated memory.

Resources: https://man7.org/linux/man-pages/man3/malloc.3.html

`os_calloc()`

The os_calloc function allocates memory and initializes it to zero. 
It first checks if the size arguments are not zero and then calculates 
the total size required for the allocation. If this is the first 
allocation, it determines whether to use mmap or sbrk. If it's not the 
first allocation, it checks if mmap is required. If mmap is required, 
it allocates a new block of memory using mmap and initializes its 
metadata. Otherwise, it allocates memory using os_malloc and 
initializes the memory to zero.

Resources: 

1. https://man7.org/linux/man-pages/man2/getpagesize.2.html
2. https://man7.org/linux/man-pages/man3/calloc.3p.html
3. https://stackoverflow.com/questions/46853908/how-to-implement-calloc

`os_realloc()`

The os_realloc function resizes a block of memory previously 
allocated by os_malloc. If the size of the requested memory is 
smaller than or equal to the previous size, it tries to shrink 
the block if possible. If the new size is larger than the previous 
size, it attempts to allocate a new block of memory and copy the 
contents of the old block to the new one, freeing the old block. 
If the ptr argument is NULL, it calls os_malloc to allocate memory 
of size size. If size is zero, it calls os_free to free the memory 
allocated by ptr and returns NULL.

Resources: 

1. https://man7.org/linux/man-pages/man3/realloc.3p.html
2. https://codereview.stackexchange.com/questions/151019/implementing-realloc-in-c

`os_free()`

The os_free function frees the memory pointed to by the given 
pointer ptr. It checks if the pointer is not NULL and then searches 
for the corresponding block of memory in the data structure. 
If the block is found, it updates the block's status to free. 
If the block is not found, it may be invalid or already freed, 
in which case it raises an error. If the pointer points to the 
pre-allocated block, it unmaps the block and sets the data_block 
pointer to NULL.

Resources: https://man7.org/linux/man-pages/man1/free.1.html

### Other Functions

`os_split_block()`

The os_split_block function is used to split a memory block 
into two smaller blocks if the original block is larger than 
the requested size and can be split. It does this by creating 
a new metadata structure for the second block and updating the 
metadata for the original block to reflect its new, smaller 
size and the presence of the new block.

`setup_data_block()`

This function sets up the initial memory block for the 
memory allocator. If the requested size is larger than 
a threshold, it allocates memory using mmap(), otherwise, 
it allocates memory using sbrk(). It creates a metadata 
block for the allocated memory and sets its size and status, 
and returns a pointer to the allocated memory block plus the 
size of the metadata block.

_________________________________________________________________________

### General observations

* In retrospect, I wish I had spent more time testing and debugging 
the code to ensure its stability.

* The project helped me a lot to realize where I have gaps and 
what I need to deepen in terms of working with memory

* There are many optimizations that I could do. Also, the 
logic behind os_split_block() doesn't seem the most efficient to me

_________________________________________________________________________

### General Resources:

1. https://stackoverflow.com/questions/32309299/how-to-align-memory-address
2. https://linux.die.net/man/2/sbrk
3. https://www3.cs.stonybrook.edu/~youngkwon/cse320/Lecture07_Dynamic_Memory_Allocation2.pdf
