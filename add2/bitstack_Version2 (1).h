#ifndef BITSTACK_H
#define BITSTACK_H

/*
 * bitstack.h
 * Low-level bitstack module (packed uint64 blocks).
 * Provides atomic set/get operations and utility functions.
 *
 * Note: push/pop semantics require concurrency control (not provided here as lock-free).
 * This module gives building blocks (atomic bit set/clear/test).
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BitStacks BitStacks;

/* create/free */
BitStacks *bitstacks_create(int *dims, int nd, int bits_per_stack);
void bitstacks_free(BitStacks *bs);

/* atomic set a bit in a stack */
void bitstacks_set(BitStacks *bs, const int *coords, int p, int bit_idx, int value);
/* get a bit (non-atomic read) */
int bitstacks_get(BitStacks *bs, const int *coords, int p, int bit_idx);

/* convenience: clear entire stack (sets all bits to zero) */
void bitstacks_clear_stack(BitStacks *bs, const int *coords, int p);

/* info */
size_t bitstacks_total_stacks(const BitStacks *bs);

#ifdef __cplusplus
}
#endif

#endif // BITSTACK_H