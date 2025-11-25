#include "headers/utils.h"
#include "headers/common.h"
#include <string.h>

// Hash function for directory table
unsigned int hash_string(const char* str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash % MAX_DIR_ENTRIES;
}

// Find entry in directory
DirEntry* find_entry_in_directory(DirNode* dir, const char* name) {
    if (dir == NULL || dir->table == NULL || name == NULL) {
        return NULL;
    }

    unsigned int hash = hash_string(name);
    DirEntry* entry = dir->table->entries[hash];
    
    while (entry != NULL) {
        if (strcmp(entry->name, name) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

// Add entry to directory
int add_entry_to_directory(DirNode* dir, const char* name, uint16_t fcb_index, int is_directory) {
    if (dir == NULL || dir->table == NULL || name == NULL) {
        return ERROR_INVALID_INPUT;
    }

    // Check if entry already exists
    if (find_entry_in_directory(dir, name) != NULL) {
        return ERROR_FILE_EXISTS;
    }

    DirEntry* new_entry = (DirEntry*)malloc(sizeof(DirEntry));
    if (new_entry == NULL) {
        return ERROR_INVALID_INPUT;
    }

    strncpy(new_entry->name, name, MAX_FILENAME - 1);
    new_entry->name[MAX_FILENAME - 1] = '\0';
    new_entry->fcb_index = fcb_index;
    new_entry->is_directory = is_directory ? 1 : 0;
    
    unsigned int hash = hash_string(name);
    new_entry->next = dir->table->entries[hash];
    dir->table->entries[hash] = new_entry;
    dir->table->count++;

    return SUCCESS;
}

// Remove entry from directory
int remove_entry_from_directory(DirNode* dir, const char* name) {
    if (dir == NULL || dir->table == NULL || name == NULL) {
        return ERROR_INVALID_INPUT;
    }

    unsigned int hash = hash_string(name);
    DirEntry* entry = dir->table->entries[hash];
    DirEntry* prev = NULL;

    while (entry != NULL) {
        if (strcmp(entry->name, name) == 0) {
            if (prev == NULL) {
                dir->table->entries[hash] = entry->next;
            } else {
                prev->next = entry->next;
            }
            free(entry);
            dir->table->count--;
            return SUCCESS;
        }
        prev = entry;
        entry = entry->next;
    }
    return ERROR_FILE_NOT_FOUND;
}

// Get FCB by index
FCB* get_fcb(uint16_t index) {
    if (disk == NULL || index >= FCB_BLOCKS) {
        return NULL;
    }
    return &disk->fcb_array[index];
}

// Allocate an FCB
uint16_t allocate_fcb(void) {
    if (disk == NULL) {
        return 0xFFFF; // Error
    }

    for (uint16_t i = 0; i < FCB_BLOCKS; i++) {
        FCB* fcb = &disk->fcb_array[i];
        // Check if FCB is free (size == 0 and all blocks are 0)
        if (fcb->size == 0 && fcb->direct_blocks[0] == 0) {
            // Initialize FCB
            memset(fcb, 0, sizeof(FCB));
            fcb->owner_id = getuid();
            fcb->permissions = PERM_READ | PERM_WRITE;
            uint32_t current_time = get_current_time();
            fcb->access_time = current_time;
            fcb->modify_time = current_time;
            fcb->change_time = current_time;
            return i;
        }
    }
    return 0xFFFF; // No free FCB
}

// Free an FCB
void free_fcb(uint16_t index) {
    if (disk == NULL || index >= FCB_BLOCKS) {
        return;
    }

    FCB* fcb = &disk->fcb_array[index];
    
    // Free all blocks associated with this FCB
    for (int i = 0; i < MAX_DIRECT_BLOCKS; i++) {
        if (fcb->direct_blocks[i] != 0) {
            free_block(fcb->direct_blocks[i]);
        }
    }
    if (fcb->indirect_block_1 != 0) {
        free_block(fcb->indirect_block_1);
    }
    if (fcb->indirect_block_2 != 0) {
        free_block(fcb->indirect_block_2);
    }

    // Clear FCB
    memset(fcb, 0, sizeof(FCB));
}

// Update FCB timestamps
void update_fcb_times(FCB* fcb, int access, int modify, int change) {
    if (fcb == NULL) {
        return;
    }

    uint32_t current_time = get_current_time();
    if (access) {
        fcb->access_time = current_time;
    }
    if (modify) {
        fcb->modify_time = current_time;
    }
    if (change) {
        fcb->change_time = current_time;
    }
}

// Get absolute path for a directory
char* get_absolute_path(DirNode* dir) {
    if (dir == NULL) {
        return NULL;
    }

    static char path[MAX_PATH_LENGTH];
    char temp_path[MAX_PATH_LENGTH];
    temp_path[0] = '\0';

    DirNode* current = dir;
    while (current != NULL) {
        char temp[MAX_PATH_LENGTH];
        if (strcmp(current->name, "/") == 0) {
            strcpy(temp, "/");
        } else {
            snprintf(temp, MAX_PATH_LENGTH, "/%s", current->name);
        }
        strcpy(temp_path, temp);
        strcat(temp_path, path);
        strcpy(path, temp_path);
        current = current->parent;
    }

    if (path[0] == '\0') {
        strcpy(path, "/");
    }

    return path;
}

// Get or create a directory node from FCB index
DirNode* get_or_create_dir_node(uint16_t fcb_index, const char* name, DirNode* parent) {
    if (disk == NULL) {
        return NULL;
    }

    FCB* fcb = get_fcb(fcb_index);
    if (fcb == NULL || fcb->file_type != FILE_TYPE_DIRECTORY) {
        return NULL;
    }

    // Check if directory node already exists in cache
    if (disk->dir_node_cache[fcb_index] != NULL) {
        return disk->dir_node_cache[fcb_index];
    }

    // Create directory node
    DirNode* dir_node = (DirNode*)malloc(sizeof(DirNode));
    if (dir_node == NULL) {
        return NULL;
    }

    if (name != NULL) {
        strncpy(dir_node->name, name, MAX_FILENAME - 1);
        dir_node->name[MAX_FILENAME - 1] = '\0';
    } else {
        strcpy(dir_node->name, "");
    }

    dir_node->parent = parent;
    dir_node->fcb_index = fcb_index;
    
    // Initialize directory table
    dir_node->table = (DirectoryTable*)malloc(sizeof(DirectoryTable));
    if (dir_node->table == NULL) {
        free(dir_node);
        return NULL;
    }
    memset(dir_node->table->entries, 0, MAX_DIR_ENTRIES * sizeof(DirEntry*));
    dir_node->table->count = 0;

    // Cache the directory node
    disk->dir_node_cache[fcb_index] = dir_node;

    return dir_node;
}

