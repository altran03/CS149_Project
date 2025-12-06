#include "headers/utils.h"
#include "headers/common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_INODES ((INODE_END - INODE_START + 1) * (BLOCK_SIZE_BYTES / sizeof(Inode)))
#define MAX_DATA_BLOCKS (DATA_END - DATA_START + 1)
#define INODE_OFFSET 1
#define INODE_BITMAP_SIZE (MAX_INODES / 8)

extern uint8_t HARD_DISK[BLOCK_NUM][BLOCK_SIZE_BYTES];

int is_bit_set(uint8_t *bitmap, uint16_t index)
{
    uint16_t byte_index = index / 8;
    uint8_t bit_index = index % 8;
    return (bitmap[byte_index] >> bit_index) & 1;
}

void set_bit(uint8_t *bitmap, uint16_t index)
{
    uint16_t byte_index = index / 8;
    uint8_t bit_index = index % 8;
    bitmap[byte_index] |= (1 << bit_index);
}

void clear_bit(uint8_t *bitmap, uint16_t index)
{
    uint16_t byte_index = index / 8;
    uint8_t bit_index = index % 8;
    bitmap[byte_index] &= ~(1 << bit_index);
}

uint8_t* get_inode_bitmap()
{
    return HARD_DISK[FREE_INODE_BITMAP];
}

uint8_t* get_data_bitmap()
{
    return HARD_DISK[FREE_DATA_BITMAP];
}

uint16_t find_free_inode()
{
    uint8_t *inode_bitmap = get_inode_bitmap();
    for (uint16_t i = INODE_OFFSET; i < MAX_INODES; i++)
    {
        if (!is_bit_set(inode_bitmap, i))
        {
            set_bit(inode_bitmap, i);
            return i;
        }
    }
    return 0;
}

uint16_t find_free_data_block()
{
    uint8_t *data_bitmap = get_data_bitmap();
    for (uint16_t i = 0; i < MAX_DATA_BLOCKS; i++)
    {
        if (!is_bit_set(data_bitmap, i))
        {
            set_bit(data_bitmap, i);
            return DATA_START + i;
        }
    }
    return 0;
}

void free_inode(uint16_t inode_number)
{
    if (inode_number == 0) return;
    uint8_t *inode_bitmap = get_inode_bitmap();
    clear_bit(inode_bitmap, inode_number);
}

void free_data_block(uint16_t block_number)
{
    if (block_number < DATA_START || block_number > DATA_END) return;
    uint8_t *data_bitmap = get_data_bitmap();
    uint16_t bitmap_index = block_number - DATA_START;
    clear_bit(data_bitmap, bitmap_index);
    memset(HARD_DISK[block_number], 0, BLOCK_SIZE_BYTES);
}
