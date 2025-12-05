#ifndef FILE_OPERATIONS_H
#define FILE_OPERATIONS_H

#include "common.h"

// Function declarations
uint16_t create_file(const char *filename);
int search_files_by_name(const char *search_path, const char *pattern, char results[][MAX_PATH_LENGTH], int max_results);

// File system operations
int fs_open(const char *pathname, uint16_t operation);
int fs_close(uint16_t file_descriptor);
int fs_read(uint16_t file_descriptor, void *buffer, size_t count);
int fs_write(uint16_t file_descriptor, const void *buffer, size_t count);

// File system initialization
void reset_hard_disk(void);
void create_root_directory(void);

#endif