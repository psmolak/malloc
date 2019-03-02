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
#include <unistd.h>

#include "malloc.h"
#include "structs.h"
#include "arena.h"

block_t *block_coalesce_forward(block_t *block);
block_t *block_find_free(ma_list_t *arenas, size_t alignment, size_t size);
block_t *block_free_extract(block_t *block, size_t alignment, size_t size);
void block_deallocate(arena_t *arena, block_t *block);
block_t *block_shrink(block_t *block, size_t size);
block_t *block_expand(block_t *block, size_t size);

#define BLOCK_IS_FREE(block) \
  ((block)->size > 0 ? true : false)

#define BLOCK_IS_ALLOCATED(block) \
  (!BLOCK_IS_FREE(block))

#define BLOCK_SET_ALLOCATED(block) \
  do { \
    ((block)->size) = -abs((block)->size); \
    BLOCK_TAG_UPDATE(block); \
  } while(0)

#define BLOCK_SET_FREE(block) \
  do { \
    ((block)->size) = abs((block)->size); \
    BLOCK_TAG_UPDATE(block); \
  } while(0)

#define BLOCK_ALIGNMENT \
  (sizeof(void *) * 2)

#define BLOCK_TAG_SIZE \
  (sizeof(mb_tag_t))

#define BLOCK_TAG_PTR(block) \
  ((mb_tag_t *)((void *)(block->data) + abs((block)->size)))

#define BLOCK_TAG(block) \
  (*((mb_tag_t *)BLOCK_TAG_PTR(block)))

/* for a given block pointer, return total size in bytes including tags */
#define BLOCK_TOTAL_SIZE(block) \
  (abs((block)->size) + 2*BLOCK_TAG_SIZE)

#define BLOCK_DATA_SIZE_FROM_TOTAL_SIZE(total) \
  ((total) - 2*BLOCK_TAG_SIZE)

/* total block minimum size including tags */
#define BLOCK_REQUIRED_MIN_SIZE \
  (2*BLOCK_TAG_SIZE + BLOCK_REQUIRED_DATA_MIN_SIZE)

#define BLOCK_REQUIRED_DATA_MIN_SIZE \
  (BLOCK_ALIGNMENT)

/* given size of data, calculate total size required for block */
#define BLOCK_REQUIRED_SIZE(size) \
  (BLOCK_TAG_SIZE + align(size, BLOCK_ALIGNMENT) + BLOCK_TAG_SIZE)

/* Given total available space, check if can fit block of specified size */
#define BLOCK_CAN_FIT_IN(total, size) \
  ((total) - 2*BLOCK_TAG_SIZE >= size)

/*
 * Calculates size of front padding block, so that following block.data
 * is aligned at 'alignment'.
 */
#define BLOCK_REQUIRED_PADDING_SIZE(alignment, block) \
  (aligned((block)->data, alignment)                  \
   ? 0                                                \
   : ((void *)(align((intptr_t)(block)                \
          + BLOCK_REQUIRED_MIN_SIZE                   \
          + BLOCK_TAG_SIZE, alignment)                \
     - BLOCK_TAG_SIZE) - (void *)(block)))

/* Update block's tag */
#define BLOCK_TAG_UPDATE(block) \
  (*((mb_tag_t *)((void *)(block->data) + abs((block)->size))) = ((block)->size))

/* Given data ptr return pointer to block */
#define BLOCK_FROM_DATA_PTR(ptr) \
  ((block_t *)((void *)((void *)(ptr) - BLOCK_TAG_SIZE)))

/* Given ending tag pointer return address of block */
#define BLOCK_FROM_TAG_PTR(tag) \
  ((block_t *)((void *)(tag) - abs(*(tag)) - BLOCK_TAG_SIZE))

/* Returns address of prev block tag */
#define BLOCK_PREV_TAG_PTR(block) \
  ((mb_tag_t *)((void *)(block) - BLOCK_TAG_SIZE))

/* Returns previous block following tags, NULL if block is first on the arena */
#define BLOCK_PREV(block) \
  (*((mb_tag_t *)BLOCK_PREV_TAG_PTR(block)) == 0 \
   ? NULL \
   : BLOCK_FROM_TAG_PTR(BLOCK_PREV_TAG_PTR(block)))

/* Returns next block following tags, NULL if block is last on the arena */
#define BLOCK_NEXT(block) \
  ((*(mb_tag_t *)((void *)(block) + BLOCK_TOTAL_SIZE(block)) == 0) \
   ? NULL \
   : ((void *)(block) + BLOCK_TOTAL_SIZE(block)))
