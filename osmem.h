// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include "printf.h"
#include "helpers.h"

void *os_malloc(size_t size);
void os_free(void *ptr);
void *os_calloc(size_t nmemb, size_t size);
void *os_realloc(void *ptr, size_t size);
void *get_block_ptr(void *ptr);
void os_split_block(struct block_meta *block_ptr, size_t size);
void *setup_data_block(size_t size);
void coalesce_prev(struct block_meta *block, struct block_meta *prev);
void coalesce_next(struct block_meta *block, struct block_meta *next);
