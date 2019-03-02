#include "invariants.h"

void assert_allocated_block(block_t *block) {
  assert(BLOCK_IS_ALLOCATED(block));
  assert(aligned(block->data, BLOCK_ALIGNMENT));
  assert(block->size == BLOCK_TAG(block));
  assert(block == BLOCK_FROM_TAG_PTR(BLOCK_TAG_PTR(block)));
}

void assert_free_block(block_t *block) {
  assert(BLOCK_IS_FREE(block));
  assert(aligned(block->data, BLOCK_ALIGNMENT));
  assert(block->size == BLOCK_TAG(block));
  assert(block == BLOCK_FROM_TAG_PTR(BLOCK_TAG_PTR(block)));
}

void assert_small_arena(arena_t *arena) {
  block_t *block = ARENA_SMALL_FIRST_BLOCK(arena);

  assert(arena->kind == SMALL);
  assert(pagealigned(arena));
  assert(ARENA_FIRST_NULL_TAG(arena) == 0);
  assert(ARENA_LAST_NULL_TAG(arena) == 0);
  assert(ARENA_PTR_IN_BOUNDS(arena, block));

  block = ARENA_SMALL_FIRST_BLOCK(arena);
  while (BLOCK_NEXT(block) != NULL) {
    block = BLOCK_NEXT(block);
    BLOCK_IS_FREE(block)
      ? assert_free_block(block)
      : assert_allocated_block(block);
  }
  assert(block == ARENA_SMALL_LAST_BLOCK(arena));

  block = ARENA_SMALL_LAST_BLOCK(arena);
  while (BLOCK_PREV(block) != NULL) {
    block = BLOCK_PREV(block);
    BLOCK_IS_FREE(block)
      ? assert_free_block(block)
      : assert_allocated_block(block);
  }
  assert(block == ARENA_SMALL_FIRST_BLOCK(arena));
}

void assert_small_new_arena(arena_t *arena) {
  block_t *block = ARENA_SMALL_FIRST_BLOCK(arena);

  assert_small_arena(arena);

  assert(LIST_FIRST(&arena->freeblks) == block);
  assert(block->size == *ARENA_SMALL_LAST_BLOCK_TAG_PTR(arena));
  assert_free_block(block);
  assert(ARENA_EMPTY(arena));
}

void assert_big_arena(arena_t *arena, size_t alignment, size_t size) {
  assert(arena->kind == BIG);
  assert(pagealigned(arena));
  assert(aligned(arena->data, alignment));
  assert(arena->datasize >= size);
  assert(ARENA_PTR_IN_BOUNDS(arena, arena->data));
}
