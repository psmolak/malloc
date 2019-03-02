#include "malloc.h"
#include "arena.h"
#include "block.h"
#include "invariants.h"

#include <sys/queue.h>
#include <pthread.h>
#include <stddef.h>

void *__my_memalign(size_t alignment, size_t size);
void *__my_realloc(void *ptr, size_t size);
void __my_free(void *ptr);

#define LOCK() if ((status = pthread_mutex_lock(&mtx))) { debug("Failed to lock. %s", strerror(status)); assert(false); }
#define UNLOCK() if ((status = pthread_mutex_unlock(&mtx))) { debug("Failed to unlock. %s", strerror(status)); assert(false); }

static int status;

static pthread_mutex_t mtx;

static arenas_t arenas = {
  .small = &(ma_list_t){},
  .big = &(ma_list_t){}
};

__constructor void __malloc_init(void) {
  __malloc_debug_init();

  pthread_mutex_init(&mtx, NULL);

  LIST_INIT(arenas.small);
  LIST_INIT(arenas.big);
}

void *__my_malloc(size_t size) {
  debug("%s(%lu)", __func__, size);
  return __my_memalign(BLOCK_ALIGNMENT, size);
}

void *__my_realloc(void *ptr, size_t size) {
  debug("%s(%p, %ld)", __func__, ptr, size);

  if (ptr == NULL)
    return __my_malloc(size);

  if (size == 0)
    return __my_free(ptr), NULL;

  arena_t *arena;
  block_t *block;

  LOCK();

  /* validate & find what arena ptr belongs to */
  if ((arena = arena_validate_ptr(arenas, ptr)) == NULL) {
    debug("Invalid ptr = %p, doesn't belong to any arena", ptr);
    exit(EXIT_FAILURE);
  }

  if (arena->kind == BIG) {
    if ((arena = arena_big_realloc(arena, size)) == NULL) {
      UNLOCK();
      errno = ENOMEM;
      return NULL;
    }
    UNLOCK();
    return arena->data;
  }

  /* Just in case someone tried to shrink too much */
  size = max(BLOCK_REQUIRED_DATA_MIN_SIZE, size);

  ma_kind_t newkind = ARENA_WHAT_KIND_REQUIRED(BLOCK_ALIGNMENT, size);
  block = BLOCK_FROM_DATA_PTR(ptr);

  if (newkind == BIG) {
    arena_t *new;
    if ((new = arena_big_allocate(BLOCK_ALIGNMENT, size)) == NULL) {
      UNLOCK();
      errno = ENOMEM;
      return NULL;
    }
    memcpy(new->data, block->data, abs(block->size));
    LIST_INSERT_HEAD(arenas.big, new, link);
    block_deallocate(arena, block);
    UNLOCK();
    return arena->data;
  }

  if ((block = arena_small_realloc(arena, block, size)) == NULL) {
    UNLOCK();
    errno = ENOMEM;
    return NULL;
  }

  UNLOCK();
  return block->data;
}

void __my_free(void *ptr) {
  debug("%s(%p)", __func__, ptr);
  if (ptr == NULL)
    return;

  arena_t *arena;
  block_t *block;

  LOCK();

  if ((arena = arena_validate_ptr(arenas, ptr)) == NULL) {
    debug("Invalid ptr = %p, out of bands.", ptr);
    exit(EXIT_FAILURE);
  }

  if (arena->kind == BIG) {
    LIST_REMOVE(arena, link);
    arena_big_deallocate(arena);
  }
  else {
    block = BLOCK_FROM_DATA_PTR(ptr);
    block_deallocate(arena, block);
  }

  UNLOCK();
}

void *__my_memalign(size_t alignment, size_t size) {
  if (!powerof2(alignment) || !aligned(alignment, sizeof(void *))) {
    errno = EINVAL;
    return NULL;
  }

  if (size == 0)
    return NULL;

  arena_t *arena;
  block_t *block;

  alignment = max(alignment, 2 * sizeof(void *));
  ma_kind_t kind = ARENA_WHAT_KIND_REQUIRED(alignment, size);

  LOCK();

  if (kind == BIG) {
    if ((arena = arena_big_allocate(alignment, size)) == NULL) {
      UNLOCK();
      errno = ENOMEM;
      return NULL;
    }
    LIST_INSERT_HEAD(arenas.big, arena, link);
    UNLOCK();
    return arena->data;
  }

  assert(kind == SMALL);

  /* To maintain invariant, we align size to double machine word */
  size = align(size, BLOCK_ALIGNMENT);

  if ((block = block_find_free(arenas.small, alignment, size)) == NULL) {
    if ((arena = arena_small_allocate(ARENA_MAXSIZE)) == NULL) {
      UNLOCK();
      errno = ENOMEM;
      return NULL;
    }
    LIST_INSERT_HEAD(arenas.small, arena, link);
    block = ARENA_SMALL_FIRST_BLOCK(arena);
  }

  block = block_free_extract(block, alignment, size);
  LIST_REMOVE(block, link);
  BLOCK_SET_ALLOCATED(block);

  UNLOCK();
  return block->data;
}

size_t __my_malloc_usable_size(void *ptr) {
  debug("%s(%p)", __func__, ptr);
  arena_t *arena;
  block_t *block;
  size_t usable_size;

  LOCK();

  /* validate & find what arena ptr belongs to */
  if ((arena = arena_validate_ptr(arenas, ptr)) == NULL) {
    debug("Invalid ptr = %p, doesn't belong to any arena", ptr);
    exit(EXIT_FAILURE);
  }

  if (arena->kind == BIG) {
    usable_size = arena->datasize;
  }
  else {
    block = BLOCK_FROM_DATA_PTR(ptr);
    usable_size = block->size;
  }

  UNLOCK();
  return usable_size;
}

/* DO NOT remove following lines */
__strong_alias(__my_free, cfree);
__strong_alias(__my_free, free);
__strong_alias(__my_malloc, malloc);
__strong_alias(__my_malloc_usable_size, malloc_usable_size);
__strong_alias(__my_memalign, aligned_alloc);
__strong_alias(__my_memalign, memalign);
__strong_alias(__my_realloc, realloc);
