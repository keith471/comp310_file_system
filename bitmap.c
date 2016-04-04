// free bitmap for OS file systems assignment

#include "bitmap.h"

#include <strings.h>    // for `ffs`

/**
 * Sets a specific bit as used
 * index is the number of the bit that we wish to set as used
 */
void force_set_index(uint32_t index) {
    int i = index / 8; // i is the index of the entry of the free_bit_map we wish to change
    int which_bit = index % 8;
    USE_BIT(free_bit_map[i], which_bit);
}

/**
 * Gets the number of the first available free bit in the bitmap
 */
uint32_t get_index() {
    uint32_t i = 0;

    // find the first section with a free bit
    // let's ignore overflow for now...
    while (free_bit_map[i] == 0) { i++; }

    // now, find the first free bit
    // ffs has the lsb as 1, not 0. So we need to subtract
    uint8_t bit = ffs(free_bit_map[i]) - 1;

    // set the bit to used
    USE_BIT(free_bit_map[i], bit);

    //return which bit we used
    return i*8 + bit;
}

/**
 * Frees the bit with number "index"
 */
void rm_index(uint32_t index) {

    // get index in array of which bit to free
    uint32_t i = index / 8;

    // get which bit to free
    uint8_t bit = index % 8;

    // free bit
    FREE_BIT(free_bit_map[i], bit);
}
