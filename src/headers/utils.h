// this is where you can declare helper functions and utility functions
#ifndef UTILS_H
#define UTILS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>
#include "common.h"

// Bitmap manipulation functions
int is_bit_set(uint8_t *bitmap, uint16_t index);
void set_bit(uint8_t *bitmap, uint16_t index);
void clear_bit(uint8_t *bitmap, uint16_t index);

// Bitmap access functions (bitmap stored in HARD_DISK[FREE_BITMAP])
uint8_t* get_inode_bitmap();
uint8_t* get_data_bitmap();

// Allocation functions
uint16_t find_free_inode();
uint16_t find_free_data_block();

// Deallocation functions
void free_inode(uint16_t inode_number);
void free_data_block(uint16_t block_number);
#endif