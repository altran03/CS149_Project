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
#define MAX_DATA_BLOCKS (DATA_END - DATA_START + 1)                                     // 15870
#define INODE_OFFSET 1                                                                  // Often inode 0 is reserved
#define INODE_BITMAP_SIZE (MAX_INODES / 8)                                              // 2048 bytes - uses entire block 1
#define INODES_PER_BLOCK BLOCK_SIZE_BYTES / sizeof(Inode)                               // 512
#define FD_PER_BLOCK BLOCK_SIZE_BYTES / sizeof(FileDescriptor)                               // 512

#define MAX_FILEDESCRIPTOR 4096
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

// Get pointer to inode bitmap in hard disk (block FREE_INODE_BITMAP = 1)
uint8_t *get_inode_bitmap()
{
    return HARD_DISK[FREE_INODE_BITMAP];
}

// Get pointer to data bitmap in hard disk (block FREE_DATA_BITMAP = 2)
uint8_t *get_data_bitmap()
{
    return HARD_DISK[FREE_DATA_BITMAP];
}

uint8_t *get_kernel_bitmap()
{
    return HARD_DISK[FREE_KERNEL_BITMAP];
}

uint16_t find_free_file_descriptor()
{
    uint8_t *kernel_bitmap = get_kernel_bitmap();
    // Start from first non-reserved file_descriptor
    for (uint16_t i = 1; i < MAX_FILEDESCRIPTOR; i++)
    {
        if (!is_bit_set(kernel_bitmap, i))
        {
            set_bit(kernel_bitmap, i); // Mark as allocated
            return i;
        }
    }
    return 0; // 0 indicates no free file descriptors (since FD 0 is reserved)
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
    uint8_t *data_bitmap = get_data_bitmap();
    // Start from first non-reserved inode
    for (uint16_t i = DATA_START; i < DATA_END; i++)
    {
        if (!is_bit_set(data_bitmap, i))
        {
            set_bit(data_bitmap, i); // Mark as allocated
            return i;
        }
    }
    return 0; // 0 indicates no free data blocks (since inode 0 is reserved)
}

void free_inode(uint16_t inode_number)
{
    if (inode_number == 0)
        return; // Don't free reserved inode 0
    uint8_t *inode_bitmap = get_inode_bitmap();
    clear_bit(inode_bitmap, inode_number);
}

void free_file_descriptor(uint16_t fd)
{
    if (fd == 0)
        return; // Don't free file descriptor 0
    uint8_t *kernel_bitmap = get_kernel_bitmap();
    clear_bit(kernel_bitmap, fd);
}

void free_data_block(uint16_t block_number)
{
    if (block_number < DATA_START || block_number > DATA_END)
        return;
    // Clear the block by zeroing it out
    memset(HARD_DISK[block_number], 0, BLOCK_SIZE_BYTES);
}

//puts block_number and byte_offset into result array
void inode_number_to_disk_location(uint16_t inode_num, uint16_t result[2])
{
    if (inode_num < 0 || inode_num > MAX_INODES) {
        printf("Invalid Inode Number: %d\n", inode_num);
        return;
    }
    uint16_t block_num; // return parameters block_num and byte_offset
    uint16_t byte_offset;
    // Calculate which block contains this inode
    block_num = inode_num / INODES_PER_BLOCK + INODE_START;

    // Calculate offset within the block
    uint16_t inode_index_in_block = inode_num % INODES_PER_BLOCK;
    byte_offset = inode_index_in_block * sizeof(Inode);
    
    result[0] = block_num;
    result[1] = byte_offset;

    return;
}

//puts block_number and byte_offset into result array
void fd_number_to_disk_location(uint16_t fd, uint16_t result[2])
{
    //the difference to inode is the max amount
    if (fd < 0 || fd > MAX_FILEDESCRIPTOR) {
        printf("Invalid File Descriptor Number: %d\n", fd);
        return;
    }
    uint16_t block_num; // return parameters block_num and byte_offset
    uint16_t byte_offset;
    // Calculate which block contains this inode
    block_num = fd / FD_PER_BLOCK + KERNEL_MEMORY_START;

    // Calculate offset within the block
    uint16_t fd_index_in_block = fd % FD_PER_BLOCK;
    byte_offset = fd_index_in_block * sizeof(FileDescriptor);
    
    result[0] = block_num;
    result[1] = byte_offset;

    return;
}