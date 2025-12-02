#ifndef FILE_OPERATIONS_H
#define FILE_OPERATIONS_H

#include "common.h"

// Function declarations
int create_file(const char *filename);
int delete_file(const char *filename);
int copy_file(const char *source, const char *destination);
int move_file(const char *source, const char *destination);
int rename_file(const char *old_name, const char *new_name);
void display_file(const char *filename);
int append_to_file(const char *filename, const char *content);
int search_in_file(const char *filename, const char *pattern);
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