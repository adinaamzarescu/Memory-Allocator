// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"
#include "helpers.h"

const size_t MMAP_THRESHOLD = 128 * 1024;

/*
 *	Align size to the nearest multiple of ALIGNMENT.
 *	For example, if ALIGNMENT is 8,
 *	then ALIGN(5) = 8, ALIGN(8) = 8, ALIGN(9) = 16.
 */
const size_t ALIGNMENT = 8;
#define ALIGN(size)(((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

const size_t PAGE_SIZE = 4096;
const size_t META_SIZE = ALIGN(sizeof(struct block_meta));

struct block_meta *data_block;
int count;

void *get_block_ptr(void *ptr)
{
	return ptr - META_SIZE;
}

void os_split_block(struct block_meta *block_ptr, size_t size)
{
	/* Check if the block can be split into two parts */
	if (block_ptr->size - size >= META_SIZE) {
		/* Calculate the address of the new block's metadata */
		struct block_meta *new_block_ptr =
			(struct block_meta *)((char *)block_ptr + META_SIZE + size);
		/* Set the new block's metadata */
		new_block_ptr->size = block_ptr->size - size - META_SIZE;
		new_block_ptr->next = block_ptr->next;
		new_block_ptr->status = STATUS_FREE;
		/* Set the old block's metadata */
		block_ptr->size = size;
		block_ptr->next = new_block_ptr;
	}
}

void *setup_data_block(size_t size)
{
	count++;
	// Store the address of the allocated memory block
	void *result;

	if (size >= MMAP_THRESHOLD) {
		// Allocate memory using `mmap()`
		result = mmap(NULL, ALIGN(size + sizeof(struct block_meta)), PROT_READ |
					  PROT_WRITE, MAP_PRIVATE |
					  MAP_ANONYMOUS, -1, 0);
		// Check whether the memory allocation was successful
		DIE(result == MAP_FAILED, "mmap");
	} else {
		// Allocate memory using `sbrk()`
		result = sbrk(MMAP_THRESHOLD);
		// Check whether the memory allocation was successful.
		DIE(result == (void *)-1, "sbrk");
	}

	data_block = (struct block_meta *)result;
	data_block->size = size;
	data_block->status = (size >= MMAP_THRESHOLD) ? STATUS_MAPPED :
													STATUS_ALLOC;
	data_block->next = NULL;

	// Return the address of the allocated memory block
	return result + META_SIZE;
}

void coalesce_prev(struct block_meta *block, struct block_meta *prev)
{
	prev->size += block->size + sizeof(struct block_meta);
	prev->next = block->next;
}

void coalesce_next(struct block_meta *block, struct block_meta *next)
{
	block->size += next->size + sizeof(struct block_meta);
	block->next = next->next;
}

void *os_malloc(size_t size)
{
	/* If size is zero, return NULL */
	if (!size)
		return NULL;

	/* If this is the first allocation, set up the data_block */
	if (!count)
		return setup_data_block(size);

	/* Try to find a free block that's big enough */
	struct block_meta *current = data_block;
	struct block_meta *prev = NULL;
	while (current) {
		if (current->status == STATUS_FREE && current->size >= size)
			break;

		prev = current;
		current = current->next;
	}

	/* If a free block is found */
	if (current) {
		current->status = STATUS_ALLOC;
		void *ptr = (void *)current + META_SIZE;

		/* Coalesce with the previous block if it's free */
		if (prev && prev->status == STATUS_FREE) {
			coalesce_prev(current, prev);
			current = prev;
		}
		/* Coalesce with the next block if it's free */
		if (current->next && current->next->status == STATUS_FREE)
			coalesce_next(current, current->next);

		/* Zero out the memory */
		for (unsigned int i = 0; i < size; i++)
			*((char *)(ptr + i)) = 0;

		/* Split the block if it's much larger than what we need */
		if (current->size >= size + sizeof(struct block_meta) + ALIGNMENT) {
			struct block_meta *split_block =
				(struct block_meta *)((char *)current +
				ALIGN(size + sizeof(struct block_meta)));

			*split_block = (struct block_meta) {
					.size = current->size - size - sizeof(struct block_meta),
					.status = STATUS_FREE,
					.next = current->next
			};

			current->next = split_block;
			current->size = size;
		}

		return ptr;
	}

	/* Allocate a new block */
	void *res;
	if (size >= MMAP_THRESHOLD) {
		res = mmap(NULL, ALIGN(size + sizeof(struct block_meta)), PROT_READ |
				   PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		DIE(res == MAP_FAILED, "mmap");
	} else {
		res = sbrk(ALIGN(size + sizeof(struct block_meta)));
		DIE(res == (void *)-1, "sbrk");
	}

	current = (struct block_meta *)res;
	current->size = size;
	current->status = (size >= MMAP_THRESHOLD) ? STATUS_MAPPED : STATUS_ALLOC;
	current->next = NULL;

	/* Add the new block to the end of the list */
	struct block_meta *last = data_block;
	while (last->next)
		last = last->next;

	last->next = current;
	return (void *)current + META_SIZE;
}

void os_free(void *ptr)
{
	if (!ptr)
		/* If ptr is NULL, no action needed */
		return;

	/* Check if the block is the pre-allocated block */
	if (!((void *)data_block == ptr - META_SIZE &&
		  data_block->status == STATUS_MAPPED))	{
		/* Check if the block is allocated and update its status to free */
		struct block_meta *current_block = data_block;
		while (current_block) {
			if (!((void *)current_block == ptr - META_SIZE &&
				  current_block->status == STATUS_ALLOC)) {
				current_block = current_block->next;
			} else {
				current_block->status = STATUS_FREE;
				return;
			}
		}

		/* Check if the block is a mapped block and unmap it */
		current_block = data_block;
		while (current_block) {
			if ((void *)current_block->next == ptr - META_SIZE &&
				current_block->next->status == STATUS_MAPPED) {
				struct block_meta *block_to_unmap = current_block->next;
				int META_SIZE =
					ALIGN(block_to_unmap->size + sizeof(struct block_meta));

				if (current_block->next->next)
					current_block->next = current_block->next->next;
				else
					current_block->next = NULL;

				munmap(ptr - ALIGN(sizeof(struct block_meta)), META_SIZE);
				return;
			}

			current_block = current_block->next;
		}

		/* If the block is not found, it may be invalid or already freed */
		DIE(1, "Invalid block");
		return;
	}

	/* Unmap the pre-allocated block and set data_block to NULL */
	munmap(ptr - ALIGN(sizeof(struct block_meta)),
		   ALIGN(data_block->size + sizeof(struct block_meta)));
	data_block = NULL;
}

void *os_calloc(size_t nmemb, size_t size)
{
	/* Check if either argument is 0, in which case return NULL. */
	if (!nmemb || !size)
		return NULL;

	/* Calculate the total size required for the allocation. */
	size_t total_size = nmemb * size;

	/*
	 *	If this is the first allocation, determine whether
	 *	to use mmap or sbrk.
	 */
	if (!data_block) {
		if (total_size + sizeof(struct block_meta) >= PAGE_SIZE) {
			/* Use mmap to allocate a new block and initialize its metadata. */
			void *result = mmap(NULL, ALIGN(total_size + META_SIZE),
								PROT_READ | PROT_WRITE, MAP_PRIVATE |
								MAP_ANONYMOUS, -1, 0);
			DIE(result == MAP_FAILED, "mmap");

			struct block_meta *block = (struct block_meta *)result;
			block->status = STATUS_MAPPED;
			block->size = total_size;
			block->next = NULL;

			return (void *)(block + 1);
		}

		/* Use sbrk to allocate a new block and initialize its metadata. */
		void *result = sbrk(MMAP_THRESHOLD);
		DIE(result == (void *)-1, "sbrk");

		struct block_meta *block = (struct block_meta *)result;
		block->status = STATUS_ALLOC;
		block->size = total_size;
		block->next = NULL;
		data_block = block;

		return result + META_SIZE;
	}

	/* If this is not the first allocation, check if mmap is required. */
	if (total_size + sizeof(struct block_meta) >= PAGE_SIZE) {
		/* Use mmap to allocate a new block and initialize its metadata.*/
		void *result = mmap(NULL, ALIGN(total_size + META_SIZE),
							PROT_READ | PROT_WRITE, MAP_PRIVATE |
							MAP_ANONYMOUS, -1, 0);
		DIE(result == MAP_FAILED, "mmap");

		struct block_meta *block = (struct block_meta *)result;
		block->status = STATUS_MAPPED;
		block->size = total_size;
		block->next = NULL;

		/*
		 *	Traverse the linked list of allocated blocks
		 *	to find the last one.
		 */
		struct block_meta *aux = data_block;
		while (aux->next)
			aux = aux->next;

		/*
		 *	Link the new block to the end of the linked list and return
		 *	a pointer to the data section.
		 */
		aux->next = block;
		return result + META_SIZE;
	}

	/*
	 *	If mmap is not required, allocate using os_malloc and initialize
	 *	the memory to 0.
	 */
	void *ptr = os_malloc(total_size);
	if (ptr)
		memset(ptr, 0, total_size);

	return ptr;
}

void *os_realloc(void *ptr, size_t size)
{
	/* If ptr is NULL, call os_malloc() to allocate memory of size 'size' */
	if (!ptr)
		return os_malloc(size);

	/*
	 *	If size is 0, call os_free() to free the memory allocated by 'ptr'
	 *	and return NULL
	 */
	if (!size) {
		os_free(ptr);
		return NULL;
	}

	/* Get the metadata of the block pointed to by 'ptr' */
	struct block_meta *block_ptr = get_block_ptr(ptr);
	/* Get the size of the block pointed to by 'ptr' */
	size_t old_size = block_ptr->size;

	/*
	 *	If the new size is smaller than or equal to the old size, try to
	 *	shrink the block if possible.
	 */
	if (size <= old_size) {
		/* Shrink the block if possible and return the same pointer. */
		if (old_size - size >= META_SIZE)
			os_split_block(block_ptr, size);

		block_ptr->size = size;
		return ptr;
	}

	/* Get the size of the next block (if it exists and is free) */
	size_t next_block_size = 0;
	struct block_meta *next_block = block_ptr->next;
	if (next_block && next_block->status == STATUS_FREE)
		next_block_size = next_block->size + META_SIZE;

	/*
	 *	If the new size can be accommodated by combining the
	 *	current and next free blocks, do so.
	 */
	if (block_ptr->size + next_block_size >= size) {
		/* Combine with the next free block if possible. */
		os_split_block(next_block, size - block_ptr->size);
		block_ptr->size = size;
		return ptr;
	}

	/*
	 *	If none of the above conditions are true, allocate a new block of
	 *	size 'size' and copy the data from the old block to the new block.
	 */
	void *new_ptr = os_malloc(size);
	if (!new_ptr)
		return NULL;

	memcpy(new_ptr, ptr, old_size);
	os_free(ptr);
	return new_ptr;
}
