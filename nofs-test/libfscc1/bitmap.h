//
// Created by Ding Yuan on 2018-01-10.
//

#ifndef LIBFS_BITMAP_H
#define LIBFS_BITMAP_H

#include <stdint.h>
// Implement utility functions for bitmap, which is used to maintain free blocks

#define BITS_PER_BYTE  8
#define ARRAY_INDEX(b) ((b) / BITS_PER_BYTE) // Given the block number, calculate the index to the uint array
#define BIT_OFFSET(b)  ((b) % BITS_PER_BYTE) // Calculate the bit position within the located uint element

void set_bit (uint8_t *bitmap, uint32_t b);

void clear_bit (uint8_t *bitmap, uint32_t b);

int get_bit (uint8_t *bitmap, uint32_t b);
#endif //LIBFS_BITMAP_H
