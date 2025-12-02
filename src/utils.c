#include "headers/utils.h"
#include "headers/common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Bitmap is stored in HARD_DISK[FREE_BITMAP] block (block 1, 2048 bytes = 16384 bits)
// Layout: First 16384 bits for inodes, remaining bits for data blocks
// Since we have 16384 bits total and need to track 16384 inodes + 15870 data blocks,
// we'll use a compact approach: track inodes in first 16384 bits, and data blocks
// in a compressed format or use the remaining space efficiently
// 
// For now, we'll use: 
// - Bits 0-16383: inode allocation (16384 bits = 2048 bytes, uses full block)
// - For data blocks, we'll use a simple linear search or track in a second mechanism
//   Since block 1 is full with inodes, we'll implement data block tracking separately
//   or use a different allocation strategy

#define MAX_INODES ((INODE_END - INODE_START + 1) * (BLOCK_SIZE_BYTES / sizeof(Inode))) // 16384
#define MAX_DATA_BLOCKS (DATA_END - DATA_START + 1) // 15870
#define INODE_OFFSET 1 // Often inode 0 is reserved
#define INODE_BITMAP_SIZE (MAX_INODES / 8) // 2048 bytes - uses entire block 1

// External reference to hard disk (defined in dev.c)
extern uint8_t HARD_DISK[BLOCK_NUM][BLOCK_SIZE_BYTES];

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

void clear_bit(uint8_t *bitmap, uint16_t index)
{
    uint16_t byte_index = index / 8;
    uint8_t bit_index = index % 8;
    bitmap[byte_index] &= ~(1 << bit_index);
}

// Get pointer to inode bitmap in hard disk (block FREE_BITMAP = 1)
uint8_t* get_inode_bitmap()
{
    return HARD_DISK[FREE_BITMAP];
}

// Get pointer to data bitmap in hard disk
// Note: Since block 1 is full with inode bitmap, we'll use a simple approach:
// Track data blocks by checking if they're in use via their inode references
// For now, we'll use a second bitmap approach stored elsewhere or check blocks directly
// For simplicity, we'll use block SUPERBLOCK (0) for data bitmap tracking
// (assuming superblock uses minimal space, or we can use a different approach)
uint8_t* get_data_bitmap()
{
    // Use a portion of superblock or implement differently
    // For now, return NULL and implement direct block checking
    // TODO: Implement proper data block bitmap storage
    return NULL; // Will implement direct checking instead
}

uint16_t find_free_inode()
{
    uint8_t *inode_bitmap = get_inode_bitmap();
    // Start from first non-reserved inode
    for (uint16_t i = INODE_OFFSET; i < MAX_INODES; i++)
    {
        if (!is_bit_set(inode_bitmap, i))
        {
            set_bit(inode_bitmap, i); // Mark as allocated
            return i;
        }
    }
    return 0; // 0 indicates no free inodes (since inode 0 is reserved)
}

uint16_t find_free_data_block()
{
    // Since we don't have a dedicated data bitmap block, we'll use a simple approach:
    // Check blocks directly by looking for zero-filled blocks
    // This is less efficient but works with the current layout constraints
    // A better solution would be to redesign the bitmap layout
    
    // Simple linear search for a free block (all zeros indicates free)
    for (uint16_t block_num = DATA_START; block_num <= DATA_END; block_num++)
    {
        // Check if block is all zeros (free)
        int is_free = 1;
        for (int i = 0; i < BLOCK_SIZE_BYTES; i++)
        {
            if (HARD_DISK[block_num][i] != 0)
            {
                is_free = 0;
                break;
            }
        }
        if (is_free)
        {
            // Mark block as allocated by writing a marker (or use a different method)
            // For now, we'll rely on the block being non-zero when used
            return block_num;
        }
    }
    return 0; // 0 indicates no free data blocks
}

void free_inode(uint16_t inode_number)
{
    if (inode_number == 0) return; // Don't free reserved inode 0
    uint8_t *inode_bitmap = get_inode_bitmap();
    clear_bit(inode_bitmap, inode_number);
}

void free_data_block(uint16_t block_number)
{
    if (block_number < DATA_START || block_number > DATA_END) return;
    // Clear the block by zeroing it out
    memset(HARD_DISK[block_number], 0, BLOCK_SIZE_BYTES);
}