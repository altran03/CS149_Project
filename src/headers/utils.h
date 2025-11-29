// this is where you can declare helper functions and utility functions
#ifndef UTILS_H
#define UTILS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>

extern uint8_t inode_bitmap[2048]; // 2048 bytes = 16384 bits for inode allocation tracking
// Function declarations
uint16_t find_free_inode();
void set_bit(uint8_t *bitmap, uint16_t index);
int is_bit_set(uint8_t *bitmap, uint16_t index);
#endif