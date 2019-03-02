# malloc
Memory allocator for Linux

We have two kinds of arenas, one for small and one for big allocations.
Small arena consists of blocks placed contigously one after another,
starting at first valid address just after arena header. It also
maintains explicit list of available free blocks.
Big arena consists of one chunk of memory starting at 'arena.data'.
There is no notion of blocks here.
Size in both cases means total bytes allocated including arena header.
Each arena is allocated at granularity of memory page.

```
/*
 * Blocks are represented as payload with tags at both ends.
 * For simplicity, tags are 8 bytes long. Negative tag value
 * indicates allocated block whereas positive free block.
 *
 * Blocks are placed continously one after another such that
 * their data pointers are always aligned at 16. For this
 * reason, size of data inside block is also multple of 16.
 * This invariant is assumed across implementation files.
 *
 * Tags are placed on addresses such that data following tag
 * is always properly aligned.
 *
 *              ____________ block ____________
 *             /                               \
 *    ---+-----+-----+-------------------+-----+-----+---
 *       | tag | tag |      payload      | tag | tag |
 *    ---+-----+-----+-------------------+-----+-----+---
 *       |     |     |                   |     |     |
 *      16     8    16                  16     8    16
 *
 *
 * To allow safe block traversal by tags, we need extra fake
 * tags at boundary blocks inside arena. First block is always
 * preceeded with fake NUL tag, same for last block which is
 * always followed by fake NUL tag.
 *
 *           _________ first block _________
 *          /                               \
 *    +-----+-----+-------------------+-----+-----+---
 *    | NUL | tag |      payload      | tag | tag |
 *    +-----+-----+-------------------+-----+-----+---
 *
 *              _________ last block __________
 *             /                               \
 *    ---+-----+-----+-------------------+-----+-----+
 *       | tag | tag |      payload      | tag | NUL |
 *    ---+-----+-----+-------------------+-----+-----+
 */
```
