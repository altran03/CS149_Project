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

#endif