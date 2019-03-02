#include "arena.h"
#include "block.h"

void assert_free_block(block_t *block);
void assert_allocated_block(block_t *block);
void assert_small_arena(arena_t *arena);
void assert_small_new_arena(arena_t *arena);
void assert_big_arena(arena_t *arena, size_t alignment, size_t size);
