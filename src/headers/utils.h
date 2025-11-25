#ifndef UTILS_H
#define UTILS_H

#include "common.h"

// Utility functions
unsigned int hash_string(const char* str);
DirEntry* find_entry_in_directory(DirNode* dir, const char* name);
int add_entry_to_directory(DirNode* dir, const char* name, uint16_t fcb_index, int is_directory);
int remove_entry_from_directory(DirNode* dir, const char* name);
FCB* get_fcb(uint16_t index);
uint16_t allocate_fcb(void);
void free_fcb(uint16_t index);
void update_fcb_times(FCB* fcb, int access, int modify, int change);
char* get_absolute_path(DirNode* dir);
DirNode* get_or_create_dir_node(uint16_t fcb_index, const char* name, DirNode* parent);

#endif