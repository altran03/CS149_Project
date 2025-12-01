#include "headers/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstdint>

#define MAX_INODES (2048 * 8)
#define INODE_OFFSET 1 // Often inode 0 is reserved

int is_bit_set(uint8_t *bitmap, uint16_t index)
{
    uint16_t byte_index = index / 8;              // array index represents bytes
    uint8_t bit_index = index % 8;                // bit position within that byte
    return (bitmap[byte_index] >> bit_index) & 1; // return 1 if bit is set to 1, 0 otherwise
}
void set_bit(uint8_t *bitmap, uint16_t index)
{
    uint16_t byte_index = index / 8;
    uint8_t bit_index = index % 8;
    bitmap[byte_index] |= (1 << bit_index);
}
uint16_t find_free_inode()
{
    // Start from first non-reserved inode
    for (uint16_t i = INODE_OFFSET; i < MAX_INODES; i++)
    {
        if (!is_bit_set(inode_bitmap, i))
        {                             // needs to be defined in utils.h
            set_bit(inode_bitmap, i); // Mark as allocated
            return i;
        }
    }
    return 0; // 0 indicates no free inodes (since inode 0 is reserved)
}