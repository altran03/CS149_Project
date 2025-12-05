// this is where we will declare functions for directory operations
#include <stdio.h>

#ifndef DIRECTORY_OPERATIONS_H
#define DIRECTORY_OPERATIONS_H
#include "common.h"

// Function declarations
// Note: create_directory returns uint16_t (inode number) or 0 on error
uint16_t create_directory(const char *dirname);
void create_root_directory(void);
void init_root_inode(void);
void init_inode(uint16_t inode_number);
#endif