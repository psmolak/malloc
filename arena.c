#include "arena.h"
#include "invariants.h"

static void *get_memory(size_t size) {
  void *mem = NULL;

  int prot = PROT_READ | PROT_WRITE;
  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  if ((mem = mmap(NULL, size, prot, flags, -1, 0)) == MAP_FAILED) {
    debug("mmap failed with \"%s\"", strerror(errno));
    mem = NULL;
  }

  return mem;
}

arena_t *arena_small_allocate(size_t size) {
  arena_t *arena;
  block_t *block;

  /* FIXME: find better way to handle big size values */
  size = min(size, ((size_t)-1) - (10 * getpagesize()));

  size_t reqsize = pagealign(size);
  arena = get_memory(reqsize);

  if (arena == NULL)
    return NULL;

  arena->kind = SMALL;
  arena->size = reqsize;
  LIST_INIT(&arena->freeblks);
  ARENA_SMALL_SET_NULL_TAGS(arena);

  block = ARENA_SMALL_FIRST_BLOCK(arena);
  block->size = ARENA_SMALL_FIRST_BLOCK_SIZE(reqsize);
  BLOCK_TAG_UPDATE(block);
  LIST_INSERT_HEAD(&arena->freeblks, block, link);

  assert_small_new_arena(arena);

  return arena;
}

arena_t *arena_big_allocate(size_t alignment, size_t size) {
  assert(powerof2(alignment));
  assert(alignment > 0);
  assert(size > 0);

  /* FIXME: find better way to handle big size values */
  size = min(size, ((size_t)-1) - (10 * getpagesize()));

  arena_t *arena;
  size_t reqsize = pagealign(ARENA_BIG_REQUIRED_SIZE(alignment, size));

  int prot = PROT_READ | PROT_WRITE;
  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  if ((arena = mmap(NULL, reqsize, prot, flags, -1, 0)) == MAP_FAILED) {
    debug("mmap failed with \"%s\"", strerror(errno));
    return NULL;
  }

  arena->kind = BIG;
  arena->size = reqsize;
  arena->data = ARENA_BIG_DATA_PTR(alignment, arena);
  arena->datasize = ARENA_BIG_DATA_SIZE(alignment, reqsize);

  assert_big_arena(arena, alignment, size);

  return arena;
}

void arena_big_deallocate(arena_t *arena) {
  if (munmap(arena, arena->size) < 0) {
    debug("munmap failed in BIG arena deallocation");
    exit(EXIT_FAILURE);
  }
}

arena_t *arena_big_expand(arena_t *arena, size_t newsize) {
  arena_t *new;

  if ((new = arena_big_allocate(BLOCK_ALIGNMENT, newsize)) == NULL) {
    return NULL;
  }

  memcpy(new->data, arena->data, arena->datasize);
  LIST_INSERT_BEFORE(arena, new, link);
  LIST_REMOVE(arena, link);
  arena_big_deallocate(arena);

  return new;
}

arena_t *arena_big_realloc(arena_t *arena, size_t size) {
  if (size > arena->datasize)
    return arena_big_expand(arena, size);

  if (size == arena->datasize)
    return arena;

  void *addr = pagealign((void *)arena->data + size);
  ptrdiff_t diff = ((void *)arena + arena->size) - addr;

  if (diff > 0) {
    if (munmap(addr, diff) < 0) {
      debug("munmap failed with '%s'", strerror(errno));
      exit(EXIT_FAILURE);
    }
  }

  return arena;
}

static arena_t *arena_check_in_bounds(ma_list_t *arenas, void *ptr) {
  arena_t *arena;

  LIST_FOREACH(arena, arenas, link) {
    if (ARENA_PTR_IN_BOUNDS(arena, ptr))
      return arena;
  }

  return NULL;
}

/* Given list of arenas, looks for arena the ptr belongs to */
arena_t *arena_validate_ptr(arenas_t arenas, void *ptr) {
  arena_t *arena;

  if ((arena = arena_check_in_bounds(arenas.small, ptr)) == NULL)
    arena = arena_check_in_bounds(arenas.big, ptr);

  return arena;
}

void arena_insert_free_block(arena_t *arena, block_t *insert) {
  assert_free_block(insert);
  block_t *block;

  if (LIST_EMPTY(&arena->freeblks)) {
    LIST_INSERT_HEAD(&arena->freeblks, insert, link);
    return;
  }

  LIST_FOREACH(block, &arena->freeblks, link) {
    assert(insert != block);
    if (insert < block) {
      LIST_INSERT_BEFORE(block, insert, link);
      return;
    }
  }
}

uint64_t arena_total_free_size(arena_t *arena) {
  uint64_t total = 0;
  block_t *block;

  LIST_FOREACH(block, &arena->freeblks, link)
    total += (uint64_t)block->size; // free blocks thus size >= 0

  return total;
}

uint64_t arenas_total_free_size(ma_list_t *arenas) {
  uint64_t total = 0;

  arena_t *arena;
  LIST_FOREACH(arena, arenas, link)
    total += arena_total_free_size(arena);

  return total;
}

void arena_small_deallocate(arena_t *arena) {
  LIST_REMOVE(arena, link);
  if (munmap(arena, arena->size) < 0) {
    debug("munmap failed in SMALL arena deallocation");
    exit(EXIT_FAILURE);
  }
}

block_t *arena_small_realloc(arena_t *arena, block_t *block, size_t newsize) {
  /* we need to shrink our block */
  if (newsize <= abs(block->size)) {
    block_t *tail;
    if ((tail = block_shrink(block, newsize)))
      block_deallocate(arena, tail);

    return block;
  }

  /* we need to expand our block */
  block_t *expanded;
  arena_t *new;

  if ((expanded = block_expand(block, newsize)) == NULL) {
    if ((new = arena_small_allocate(ARENA_MAXSIZE)))
      return NULL;

    LIST_INSERT_AFTER(arena, new, link);
    expanded = ARENA_SMALL_FIRST_BLOCK(new);
    expanded = block_free_extract(expanded, BLOCK_ALIGNMENT, newsize);
    LIST_REMOVE(expanded, link);
    BLOCK_SET_ALLOCATED(expanded);
    assert(abs(expanded->size) >= newsize);
    memcpy(expanded->data, block->data, abs(block->size));
    block_deallocate(arena, block);
  }

  return expanded;
}
