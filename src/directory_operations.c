#include "headers/directory_operations.h"
#include "headers/utils.h"
#include "headers/common.h"
#include <string.h>

static DirNode* current_dir = NULL;

// Get current directory
DirNode* get_current_directory(void) {
    if (disk == NULL) {
        return NULL;
    }
    if (current_dir == NULL) {
        current_dir = disk->root_directory;
    }
    return current_dir;
}

// Find directory by path
DirNode* find_directory(const char* path) {
    if (disk == NULL || path == NULL) {
        return NULL;
    }

    DirNode* start_dir;
    if (path[0] == '/') {
        // Absolute path
        start_dir = disk->root_directory;
        path++; // Skip leading '/'
    } else {
        // Relative path
        start_dir = get_current_directory();
    }

    if (path[0] == '\0') {
        return start_dir;
    }

    char path_copy[MAX_PATH_LENGTH];
    strncpy(path_copy, path, MAX_PATH_LENGTH - 1);
    path_copy[MAX_PATH_LENGTH - 1] = '\0';

    char* token = strtok(path_copy, "/");
    DirNode* current = start_dir;

    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            // Current directory, do nothing
        } else if (strcmp(token, "..") == 0) {
            // Parent directory
            if (current->parent != NULL) {
                current = current->parent;
            }
        } else {
            // Find subdirectory
            DirEntry* entry = find_entry_in_directory(current, token);
            if (entry == NULL || !entry->is_directory) {
                return NULL; // Directory not found
            }
            FCB* fcb = get_fcb(entry->fcb_index);
            if (fcb == NULL || fcb->file_type != FILE_TYPE_DIRECTORY) {
                return NULL;
            }
            // Get or create directory node for this subdirectory
            DirNode* subdir = get_or_create_dir_node(entry->fcb_index, entry->name, current);
            if (subdir == NULL) {
                return NULL;
            }
            current = subdir;
        }
        token = strtok(NULL, "/");
    }

    return current;
}

// Create directory
int mkDir(const char* name) {
    if (disk == NULL || name == NULL) {
        return ERROR_INVALID_INPUT;
    }

    DirNode* current = get_current_directory();
    if (current == NULL) {
        return ERROR_INVALID_INPUT;
    }

    // Check if directory already exists
    if (find_entry_in_directory(current, name) != NULL) {
        return ERROR_FILE_EXISTS;
    }

    // Allocate FCB for new directory
    uint16_t fcb_index = allocate_fcb();
    if (fcb_index == 0xFFFF) {
        return ERROR_DISK_FULL;
    }

    FCB* fcb = get_fcb(fcb_index);
    fcb->file_type = FILE_TYPE_DIRECTORY;
    fcb->size = 0;
    fcb->owner_id = getuid();
    fcb->permissions = PERM_READ | PERM_WRITE | PERM_EXEC;
    update_fcb_times(fcb, 0, 1, 1);

    // Create directory node for the new directory
    DirNode* new_dir = get_or_create_dir_node(fcb_index, name, current);
    if (new_dir == NULL) {
        free_fcb(fcb_index);
        return ERROR_INVALID_INPUT;
    }

    // Add entry to current directory
    int result = add_entry_to_directory(current, name, fcb_index, 1);
    if (result != SUCCESS) {
        free(new_dir->table);
        free(new_dir);
        free_fcb(fcb_index);
        return result;
    }

    // Update parent directory's modify time
    FCB* parent_fcb = get_fcb(current->fcb_index);
    if (parent_fcb != NULL) {
        update_fcb_times(parent_fcb, 0, 1, 0);
    }

    return SUCCESS;
}

// Change directory
int cd(const char* path) {
    if (disk == NULL || path == NULL) {
        return ERROR_INVALID_INPUT;
    }

    DirNode* target = find_directory(path);
    if (target == NULL) {
        return ERROR_DIR_NOT_FOUND;
    }

    current_dir = target;
    
    // Update access time
    FCB* fcb = get_fcb(target->fcb_index);
    if (fcb != NULL) {
        update_fcb_times(fcb, 1, 0, 0);
    }

    return SUCCESS;
}

// List directory contents
int list_directory(const char* path) {
    if (disk == NULL) {
        return ERROR_INVALID_INPUT;
    }

    DirNode* dir;
    if (path == NULL) {
        dir = get_current_directory();
    } else {
        dir = find_directory(path);
    }

    if (dir == NULL || dir->table == NULL) {
        return ERROR_DIR_NOT_FOUND;
    }

    printf("Directory: %s\n", get_absolute_path(dir));
    printf("Total entries: %d\n\n", dir->table->count);

    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
        DirEntry* entry = dir->table->entries[i];
        while (entry != NULL) {
            FCB* fcb = get_fcb(entry->fcb_index);
            if (fcb != NULL) {
                printf("%s\t%s\t%u bytes\n",
                       entry->is_directory ? "d" : "-",
                       entry->name,
                       fcb->size);
            }
            entry = entry->next;
        }
    }

    // Update access time
    FCB* dir_fcb = get_fcb(dir->fcb_index);
    if (dir_fcb != NULL) {
        update_fcb_times(dir_fcb, 1, 0, 0);
    }

    return SUCCESS;
}

// Remove directory
int remove_directory(const char* name) {
    if (disk == NULL || name == NULL) {
        return ERROR_INVALID_INPUT;
    }

    DirNode* current = get_current_directory();
    if (current == NULL) {
        return ERROR_INVALID_INPUT;
    }

    DirEntry* entry = find_entry_in_directory(current, name);
    if (entry == NULL) {
        return ERROR_FILE_NOT_FOUND;
    }

    if (!entry->is_directory) {
        return ERROR_INVALID_INPUT; // Not a directory
    }

    // Check if directory is empty
    FCB* fcb = get_fcb(entry->fcb_index);
    if (fcb == NULL) {
        return ERROR_INVALID_INPUT;
    }

    // In a full implementation, we'd check if the directory is empty
    // For now, we'll allow removal

    // Remove entry from parent directory
    int result = remove_entry_from_directory(current, name);
    if (result != SUCCESS) {
        return result;
    }

    // Free FCB
    free_fcb(entry->fcb_index);

    // Update parent directory's modify time
    FCB* parent_fcb = get_fcb(current->fcb_index);
    if (parent_fcb != NULL) {
        update_fcb_times(parent_fcb, 0, 1, 0);
    }

    return SUCCESS;
}

