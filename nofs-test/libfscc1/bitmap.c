//
// Created by Ding Yuan on 2018-01-10.
//

#include "bitmap.h"

// Implement utility functions for bitmap, which is used to maintain free blocks

void set_bit (uint8_t *bitmap, uint32_t b) {
    bitmap[ARRAY_INDEX(b)] |= (1 << BIT_OFFSET(b));
}

void clear_bit (uint8_t *bitmap, uint32_t b) {
    bitmap[ARRAY_INDEX(b)] &= ~(1 << BIT_OFFSET(b));
}

int get_bit (uint8_t *bitmap, uint32_t b) {
    uint8_t bit = bitmap[ARRAY_INDEX(b)] & (1 << BIT_OFFSET(b));
    return bit != 0;
}
