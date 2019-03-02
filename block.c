#include "block.h"
#include "invariants.h"

static bool block_can_fit(block_t *block, size_t alignment, size_t size) {
  assert(alignment >= 16); // BLOCK_REQURIED_PADDING_SIZE

  size_t total = BLOCK_TOTAL_SIZE(block);
  size_t required = BLOCK_REQUIRED_PADDING_SIZE(alignment, block);
  size_t remaining = total - required;

  assert(aligned(remaining, BLOCK_ALIGNMENT));

  return BLOCK_CAN_FIT_IN(remaining, size);
}

block_t *block_find_free(ma_list_t *arenas, size_t alignment, size_t size) {
  block_t *block;
  arena_t *arena;

  LIST_FOREACH(arena, arenas, link) {
    LIST_FOREACH(block, &arena->freeblks, link) {
      if (block_can_fit(block, alignment, size))
        return block;
    }
  }

  return NULL;
}

/* Splits block into head of 'size' size and tail of what's left */
block_t *block_free_split(block_t *block, size_t size) {
  assert(BLOCK_IS_FREE(block));
  assert(aligned(size, BLOCK_ALIGNMENT));

  if (size == 0)
    return block;

  size_t total = BLOCK_TOTAL_SIZE(block);

  block_t *head = block;
  block_t *tail = (void *)block + size;

  head->size = BLOCK_DATA_SIZE_FROM_TOTAL_SIZE(size);
  BLOCK_TAG_UPDATE(head);
  tail->size = BLOCK_DATA_SIZE_FROM_TOTAL_SIZE(total - size);
  BLOCK_TAG_UPDATE(tail);

  assert_free_block(head);
  assert_free_block(tail);

  return tail;
}

block_t *block_free_extract(block_t *block, size_t alignment, size_t size) {
  assert_free_block(block);

  block_t *head;
  block_t *tail;

  size_t total = BLOCK_TOTAL_SIZE(block);
  size_t required = BLOCK_REQUIRED_SIZE(size);
  size_t padding = BLOCK_REQUIRED_PADDING_SIZE(alignment, block);
  size_t remaining = total - padding;
  size_t trailing = remaining - required;

  if (padding) {
    head = block;
    tail = block_free_split(head, padding);
    LIST_INSERT_AFTER(block, tail, link);
    block = tail;
  }

  if (trailing >= BLOCK_REQUIRED_MIN_SIZE) {
    head = block;
    tail = block_free_split(head, required);
    LIST_INSERT_AFTER(head, tail, link);
    block = head;
  }

  return block;
}

block_t *block_coalesce_forward(block_t *block) {
  mb_tag_t orginal = block->size;
  block_t *next = BLOCK_NEXT(block);
  assert(next);

  block->size = abs(block->size) + BLOCK_TOTAL_SIZE(next);
  if (orginal < 0)
    block->size = -block->size;
  BLOCK_TAG_UPDATE(block);

  return block;
}

void block_deallocate(arena_t *arena, block_t *block) {
  assert_allocated_block(block);

  BLOCK_SET_FREE(block);

  block_t *prev = BLOCK_PREV(block);
  block_t *next = BLOCK_NEXT(block);

  if (prev && BLOCK_IS_FREE(prev)) {
    block = block_coalesce_forward(prev);
    if (next && BLOCK_IS_FREE(next)) {
      LIST_REMOVE(next, link);
      block = block_coalesce_forward(block);
    }
  }
  else if (next && BLOCK_IS_FREE(next)) {
    LIST_INSERT_BEFORE(next, block, link);
    LIST_REMOVE(next, link);
    block = block_coalesce_forward(block);
  }
  else {
    arena_insert_free_block(arena, block);
  }

  /* TODO: We need treshold freeing here! */
}

/* It shrinks first block & returns the tail if any */
block_t *block_shrink(block_t *block, size_t size) {
  assert(abs(block->size) >= size);

  size_t total = BLOCK_TOTAL_SIZE(block);
  size_t required = BLOCK_REQUIRED_SIZE(size);
  size_t remaining = total - required;

  if (remaining < BLOCK_REQUIRED_MIN_SIZE)
    return NULL;

  block_t *head = block;
  block_t *tail = (void *)block + required;

  head->size = BLOCK_DATA_SIZE_FROM_TOTAL_SIZE(required);
  BLOCK_SET_ALLOCATED(head);
  tail->size = BLOCK_DATA_SIZE_FROM_TOTAL_SIZE(remaining);
  BLOCK_SET_ALLOCATED(tail);

  return tail;
}

/* Expands current block using block after it or NULL otherwise */
block_t *block_expand(block_t *block, size_t size) {
  block_t *next = BLOCK_NEXT(block);

  /* there's no room for expansion */
  if (!next || !BLOCK_IS_FREE(next))
    return NULL;

  size_t total = BLOCK_TOTAL_SIZE(block) + BLOCK_TOTAL_SIZE(next);
  size_t required = BLOCK_REQUIRED_SIZE(size);
  assert(required > BLOCK_TOTAL_SIZE(block));

  /* following free block is too small anyway */
  if (total < required)
    return NULL;

  size_t remaining = total - required;
  size_t diff = required - BLOCK_TOTAL_SIZE(block);

  /* not enough size for tail block, coalesce then */
  if (remaining < BLOCK_REQUIRED_MIN_SIZE
      || diff < (BLOCK_REQUIRED_MIN_SIZE)) {
    LIST_REMOVE(next, link);
    return block_coalesce_forward(block);
  }

  /* we are guanranteed the head block form split is big enough */
  block_t *tail = block_free_split(next, diff);
  LIST_INSERT_AFTER(next, tail, link);
  LIST_REMOVE(next, link);
  block = block_coalesce_forward(block);

  return block;
}
