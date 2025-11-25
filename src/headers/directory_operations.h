#ifndef DIRECTORY_OPERATIONS_H
#define DIRECTORY_OPERATIONS_H

#include "common.h"

// Directory operations
int mkDir(const char* name);
int cd(const char* path);
DirNode* get_current_directory(void);
DirNode* find_directory(const char* path);
int list_directory(const char* path);
int remove_directory(const char* name);

#endif