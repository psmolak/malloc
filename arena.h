#pragma once

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stddef.h>

#include "malloc.h"
#include "structs.h"
#include "block.h"

arena_t *arena_validate_ptr(arenas_t arenas, void *ptr);
void arena_insert_free_block(arena_t *arena, block_t *block);

uint64_t arena_total_free_size(arena_t *arena);
uint64_t arenas_total_free_size(ma_list_t *arenas);

arena_t *arena_big_allocate(size_t alignment, size_t size);
void arena_big_deallocate(arena_t *arena);

arena_t *arena_big_expand(arena_t *arena, size_t size);
arena_t *arena_big_realloc(arena_t *arena, size_t size);

arena_t *arena_small_allocate(size_t size);
void arena_small_deallocate(arena_t *arena);
block_t *arena_small_realloc(arena_t *arena, block_t *block, size_t size);


/* Maximum size of SMALL arena. */
#define ARENA_MAXSIZE (BLOCK_ALIGNMENT * 32768)

#define ARENA_TRESHOLD (ARENA_MAXSIZE / 2)

#define ARENA_MAX_FREE_FIRST_BLOCK_SIZE \
  (ARENA_MAXSIZE - ARENA_HEADER_SIZE - 4*BLOCK_TAG_SIZE)

/* Does arena consists of only one block which is free? */
#define ARENA_EMPTY(arena) \
  ((ARENA_SMALL_FIRST_BLOCK(arena) == ARENA_SMALL_LAST_BLOCK(arena)) \
   && BLOCK_IS_FREE(ARENA_SMALL_FIRST_BLOCK(arena)))

/* Size of arena header. Aligned to double machine word. */
#define ARENA_HEADER_SIZE (align(sizeof(arena_t), BLOCK_ALIGNMENT))

/* Address of arena last block tag */
#define ARENA_SMALL_LAST_BLOCK_TAG_PTR(arena) \
  ((mb_tag_t *)((void *)(arena) + ((arena)->size) - 2*BLOCK_TAG_SIZE))

/* Address of last block in the arena */
#define ARENA_SMALL_LAST_BLOCK(arena) \
  ((block_t *)BLOCK_FROM_TAG_PTR(ARENA_SMALL_LAST_BLOCK_TAG_PTR(arena)))

/* Returns pointer to first small arena block */
#define ARENA_SMALL_FIRST_BLOCK(arena) \
  ((block_t *)((void *)(arena) + ARENA_HEADER_SIZE + BLOCK_TAG_SIZE))

#define ARENA_SMALL_FIRST_BLOCK_SIZE(total) \
  ((total) - ARENA_HEADER_SIZE - 2*BLOCK_TAG_SIZE - 2*BLOCK_TAG_SIZE)

#define ARENA_FIRST_NULL_TAG(arena) \
  (*((mb_tag_t *)((void *)(arena) + ARENA_HEADER_SIZE)))

#define ARENA_LAST_NULL_TAG(arena) \
  (*((mb_tag_t *)((void *)(arena) + ((arena)->size) - BLOCK_TAG_SIZE)))

/* Sets boundary null tags */
#define ARENA_SMALL_SET_NULL_TAGS(arena) \
  do { \
    ARENA_FIRST_NULL_TAG(arena) = 0; \
    ARENA_LAST_NULL_TAG(arena) = 0; \
  } while(0)

/*
 * Given alignment and size, determine min size of bytes to allocate
 * for small arena. It assumes the first byte is aligned at page boundary
 * and alignment is power of two and >= 16.
 */
#define ARENA_SMALL_REQUIRED_SIZE(alignment, size) \
  (((ARENA_HEADER_SIZE + 2*BLOCK_TAG_SIZE) % alignment == 0) \
   /* padding block at front not needed */ \
     ? ARENA_HEADER_SIZE \
       + 2*BLOCK_TAG_SIZE \
       + align(size, BLOCK_ALIGNMENT) \
       + BLOCK_TAG_SIZE \
       + BLOCK_TAG_SIZE /* this is last block, so we need NUL tag */ \
   /* padding block needed */ \
     : align(ARENA_HEADER_SIZE \
             + 2*BLOCK_TAG_SIZE \
             + BLOCK_REQUIRED_DATA_MIN_SIZE \
             + 2*BLOCK_TAG_SIZE, (alignment)) \
       + align((size), BLOCK_ALIGNMENT) \
       + BLOCK_TAG_SIZE \
       + BLOCK_TAG_SIZE /* this is last block, so we need NUL tag */)

/* Given alignment and size, check if size fits into small arena. */
#define ARENA_FITS_IN_SMALL(alignment, size) \
  (ARENA_SMALL_REQUIRED_SIZE((alignment), (size)) <= ARENA_MAXSIZE)

/* Given alignment and size, determines kind of arena we need to use. */
#define ARENA_WHAT_KIND_REQUIRED(alignment, size) \
  (ARENA_FITS_IN_SMALL(alignment, min(ARENA_MAXSIZE, size)) ? SMALL : BIG)

#define ARENA_BIG_REQUIRED_SIZE(alignment, size) \
  (align(ARENA_HEADER_SIZE, alignment) + size)

/* Given alignment and arena, returns properly aligned data pointer */
#define ARENA_BIG_DATA_PTR(alignment, arena) \
  ((void *)(arena) + (align(ARENA_HEADER_SIZE, alignment)))

/* Given alignment and total allocated size returns size of data */
#define ARENA_BIG_DATA_SIZE(alignment, total) \
  ((total) - align(ARENA_HEADER_SIZE, alignment))

/* Given arena and ptr, checks if ptr lies in arena meomry bounds */
#define ARENA_PTR_IN_BOUNDS(arena, ptr) \
  ((void *)ptr >= (void *)(arena) && (void *)ptr <= ((void *)(arena) + (arena)->size))

