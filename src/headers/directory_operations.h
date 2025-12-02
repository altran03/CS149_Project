// this is where we will declare functions for directory operations
#include <stdio.h>

#ifndef DIRECTORY_OPERATIONS_H
#define DIRECTORY_OPERATIONS_H
#include "common.h"

// Function declarations
// Note: create_directory returns uint16_t (inode number) or 0 on error
// implementation is in dev.c
uint16_t create_directory(const char *dirname);
int delete_directory(const char *dirname);
int change_directory(const char *dirname, SessionConfig *config);
void list_directory_contents(const char *dirname, const SessionConfig *config);
#endif