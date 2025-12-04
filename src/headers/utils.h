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

// Directory entry helper functions
int add_directory_entry(uint16_t dir_inode, const char *name, uint16_t target_inode);
uint16_t find_directory_entry(uint16_t dir_inode, const char *name);
DirectoryEntry* get_directory_entry_at_offset(uint8_t *dir_data, uint16_t offset);
uint16_t get_next_directory_entry_offset(uint8_t *dir_data, uint16_t current_offset, uint16_t dir_size);

// Path traversal helper function
int traverse_path(const char *pathname, uint16_t *out_inode);

// File descriptor helper functions
int allocate_file_descriptor(uint16_t inode_number, uint16_t flags);
FileDescriptor* get_file_descriptor(uint16_t fd);
int check_permissions(uint16_t inode_number, uint16_t operation);
#endif