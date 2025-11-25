#include "headers/file_operations.h"
#include "headers/directory_operations.h"
#include "headers/utils.h"
#include "headers/common.h"
#include <string.h>

// Create a file
int createFile(const char* name) {
    if (disk == NULL || name == NULL) {
        return ERROR_INVALID_INPUT;
    }

    DirNode* current = get_current_directory();
    if (current == NULL) {
        return ERROR_INVALID_INPUT;
    }

    // Check if file already exists
    if (find_entry_in_directory(current, name) != NULL) {
        return ERROR_FILE_EXISTS;
    }

    // Allocate FCB for new file
    uint16_t fcb_index = allocate_fcb();
    if (fcb_index == 0xFFFF) {
        return ERROR_DISK_FULL;
    }

    FCB* fcb = get_fcb(fcb_index);
    fcb->file_type = FILE_TYPE_REGULAR;
    fcb->size = 0;
    fcb->owner_id = getuid();
    fcb->permissions = PERM_READ | PERM_WRITE;
    update_fcb_times(fcb, 0, 1, 1);

    // Add entry to current directory
    int result = add_entry_to_directory(current, name, fcb_index, 0);
    if (result != SUCCESS) {
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

// Open a file
int openFile(const char* name, int operation) {
    if (disk == NULL || name == NULL) {
        return ERROR_INVALID_INPUT;
    }

    if (operation != MODE_READ && operation != MODE_WRITE && operation != MODE_APPEND) {
        return ERROR_INVALID_MODE;
    }

    DirNode* current = get_current_directory();
    if (current == NULL) {
        return ERROR_INVALID_INPUT;
    }

    DirEntry* entry = find_entry_in_directory(current, name);
    if (entry == NULL) {
        return ERROR_FILE_NOT_FOUND;
    }

    if (entry->is_directory) {
        return ERROR_INVALID_INPUT; // Cannot open directory as file
    }

    FCB* fcb = get_fcb(entry->fcb_index);
    if (fcb == NULL) {
        return ERROR_INVALID_INPUT;
    }

    // Check permissions
    if (operation == MODE_READ && !(fcb->permissions & PERM_READ)) {
        return ERROR_PERMISSION_DENIED;
    }
    if ((operation == MODE_WRITE || operation == MODE_APPEND) && !(fcb->permissions & PERM_WRITE)) {
        return ERROR_PERMISSION_DENIED;
    }

    // Find free file descriptor
    int fd = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!disk->fd_table->descriptors[i].is_valid) {
            fd = i;
            break;
        }
    }

    if (fd == -1) {
        return ERROR_INVALID_INPUT; // No free file descriptors
    }

    // Initialize file descriptor
    FileDescriptor* desc = &disk->fd_table->descriptors[fd];
    desc->fcb = fcb;
    desc->mode = operation;
    desc->ref_count = 1;
    desc->is_valid = 1;

    if (operation == MODE_APPEND) {
        desc->offset = fcb->size;
    } else {
        desc->offset = 0;
    }

    disk->fd_table->count++;
    update_fcb_times(fcb, 1, 0, 0);

    return fd;
}

// Close a file
int closeFile(int fd) {
    if (disk == NULL || fd < 0 || fd >= MAX_OPEN_FILES) {
        return ERROR_INVALID_INPUT;
    }

    FileDescriptor* desc = &disk->fd_table->descriptors[fd];
    if (!desc->is_valid) {
        return ERROR_FILE_NOT_OPEN;
    }

    desc->ref_count--;
    if (desc->ref_count <= 0) {
        desc->is_valid = 0;
        desc->fcb = NULL;
        desc->mode = 0;
        desc->offset = 0;
        disk->fd_table->count--;
    }

    return SUCCESS;
}

// Search for a file
int searchFile(const char* name) {
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

    FCB* fcb = get_fcb(entry->fcb_index);
    if (fcb == NULL) {
        return ERROR_INVALID_INPUT;
    }

    printf("File found: %s\n", name);
    printf("Type: %s\n", entry->is_directory ? "Directory" : "File");
    printf("Size: %u bytes\n", fcb->size);
    printf("Owner ID: %u\n", fcb->owner_id);
    printf("Permissions: %o\n", fcb->permissions);

    return SUCCESS;
}

// Rename a file
int renameFile(const char* oldName, const char* newName) {
    if (disk == NULL || oldName == NULL || newName == NULL) {
        return ERROR_INVALID_INPUT;
    }

    DirNode* current = get_current_directory();
    if (current == NULL) {
        return ERROR_INVALID_INPUT;
    }

    DirEntry* entry = find_entry_in_directory(current, oldName);
    if (entry == NULL) {
        return ERROR_FILE_NOT_FOUND;
    }

    // Check if new name already exists
    if (find_entry_in_directory(current, newName) != NULL) {
        return ERROR_FILE_EXISTS;
    }

    // Rename the entry
    strncpy(entry->name, newName, MAX_FILENAME - 1);
    entry->name[MAX_FILENAME - 1] = '\0';

    // Update FCB change time
    FCB* fcb = get_fcb(entry->fcb_index);
    if (fcb != NULL) {
        update_fcb_times(fcb, 0, 0, 1);
    }

    // Update parent directory's modify time
    FCB* parent_fcb = get_fcb(current->fcb_index);
    if (parent_fcb != NULL) {
        update_fcb_times(parent_fcb, 0, 1, 0);
    }

    return SUCCESS;
}

// Move a file
int moveFile(const char* src, const char* dir) {
    if (disk == NULL || src == NULL || dir == NULL) {
        return ERROR_INVALID_INPUT;
    }

    DirNode* current = get_current_directory();
    if (current == NULL) {
        return ERROR_INVALID_INPUT;
    }

    // Find source file
    DirEntry* src_entry = find_entry_in_directory(current, src);
    if (src_entry == NULL) {
        return ERROR_FILE_NOT_FOUND;
    }

    // Find destination directory
    DirNode* dest_dir = find_directory(dir);
    if (dest_dir == NULL) {
        return ERROR_DIR_NOT_FOUND;
    }

    // Check if file with same name exists in destination
    if (find_entry_in_directory(dest_dir, src) != NULL) {
        return ERROR_FILE_EXISTS;
    }

    // Remove from source directory
    int result = remove_entry_from_directory(current, src);
    if (result != SUCCESS) {
        return result;
    }

    // Add to destination directory
    result = add_entry_to_directory(dest_dir, src, src_entry->fcb_index, src_entry->is_directory);
    if (result != SUCCESS) {
        // Rollback: add back to source
        add_entry_to_directory(current, src, src_entry->fcb_index, src_entry->is_directory);
        return result;
    }

    // Update timestamps
    FCB* fcb = get_fcb(src_entry->fcb_index);
    if (fcb != NULL) {
        update_fcb_times(fcb, 0, 0, 1);
    }

    FCB* src_parent_fcb = get_fcb(current->fcb_index);
    if (src_parent_fcb != NULL) {
        update_fcb_times(src_parent_fcb, 0, 1, 0);
    }

    FCB* dest_parent_fcb = get_fcb(dest_dir->fcb_index);
    if (dest_parent_fcb != NULL) {
        update_fcb_times(dest_parent_fcb, 0, 1, 0);
    }

    free(src_entry);
    return SUCCESS;
}

// Write to a file
int writeFile(int fd, const char* data, int size) {
    if (disk == NULL || data == NULL || size <= 0) {
        return ERROR_INVALID_INPUT;
    }

    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return ERROR_INVALID_INPUT;
    }

    FileDescriptor* desc = &disk->fd_table->descriptors[fd];
    if (!desc->is_valid) {
        return ERROR_FILE_NOT_OPEN;
    }

    if (desc->mode == MODE_READ) {
        return ERROR_PERMISSION_DENIED;
    }

    FCB* fcb = desc->fcb;
    
    // Calculate how many blocks we need
    uint32_t total_size = desc->offset + size;
    if (total_size > fcb->size) {
        uint32_t additional_bytes = total_size - fcb->size;
        int blocks_needed = (additional_bytes + BLOCK_SIZE - 1) / BLOCK_SIZE;
        
        // Allocate blocks (simplified - only using direct blocks)
        int blocks_allocated = 0;
        for (int i = 0; i < MAX_DIRECT_BLOCKS && blocks_allocated < blocks_needed; i++) {
            if (fcb->direct_blocks[i] == 0) {
                int block = allocate_block();
                if (block == -1) {
                    return ERROR_DISK_FULL;
                }
                fcb->direct_blocks[i] = block;
                blocks_allocated++;
            }
        }
        
        if (blocks_allocated < blocks_needed) {
            return ERROR_DISK_FULL; // Need indirect blocks for full implementation
        }
        
        fcb->size = total_size;
    }

    // Write data to blocks
    uint32_t bytes_written = 0;
    uint32_t current_offset = desc->offset;
    uint32_t size_uint = (uint32_t)size;
    
    while (bytes_written < size_uint) {
        int block_index = current_offset / BLOCK_SIZE;
        if (block_index >= MAX_DIRECT_BLOCKS) {
            break; // Would need indirect blocks
        }
        
        int block_num = fcb->direct_blocks[block_index];
        if (block_num == 0) {
            break;
        }
        
        int offset_in_block = current_offset % BLOCK_SIZE;
        uint32_t remaining = size_uint - bytes_written;
        uint32_t available = BLOCK_SIZE - offset_in_block;
        uint32_t bytes_to_write = (remaining < available) ? remaining : available;
        
        memcpy(&disk->blocks[block_num][offset_in_block],
               &data[bytes_written],
               bytes_to_write);
        
        bytes_written += bytes_to_write;
        current_offset += bytes_to_write;
        desc->offset = current_offset;
    }

    update_fcb_times(fcb, 0, 1, 0);
    return bytes_written;
}

// Read from a file
int readFile(int fd, char* buffer, int size) {
    if (disk == NULL || buffer == NULL || size <= 0) {
        return ERROR_INVALID_INPUT;
    }

    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return ERROR_INVALID_INPUT;
    }

    FileDescriptor* desc = &disk->fd_table->descriptors[fd];
    if (!desc->is_valid) {
        return ERROR_FILE_NOT_OPEN;
    }

    FCB* fcb = desc->fcb;
    
    // Check if we can read
    if (desc->offset >= fcb->size) {
        return 0; // EOF
    }

    // Calculate how many bytes to read
    uint32_t size_uint = (uint32_t)size;
    uint32_t bytes_to_read = (fcb->size - desc->offset < size_uint) ?
                             fcb->size - desc->offset : size_uint;

    // Read data from blocks
    uint32_t bytes_read = 0;
    uint32_t current_offset = desc->offset;
    
    while (bytes_read < bytes_to_read) {
        int block_index = current_offset / BLOCK_SIZE;
        if (block_index >= MAX_DIRECT_BLOCKS) {
            break; // Would need indirect blocks
        }
        
        int block_num = fcb->direct_blocks[block_index];
        if (block_num == 0) {
            break;
        }
        
        int offset_in_block = current_offset % BLOCK_SIZE;
        uint32_t remaining = bytes_to_read - bytes_read;
        uint32_t available = BLOCK_SIZE - offset_in_block;
        uint32_t bytes_to_copy = (remaining < available) ? remaining : available;
        
        memcpy(&buffer[bytes_read],
               &disk->blocks[block_num][offset_in_block],
               bytes_to_copy);
        
        bytes_read += bytes_to_copy;
        current_offset += bytes_to_copy;
        desc->offset = current_offset;
    }

    update_fcb_times(fcb, 1, 0, 0);
    return bytes_read;
}

// Delete a file
int deleteFile(const char* name) {
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

    if (entry->is_directory) {
        return ERROR_INVALID_INPUT; // Use remove_directory for directories
    }

    // Check if file is open
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (disk->fd_table->descriptors[i].is_valid &&
            disk->fd_table->descriptors[i].fcb == get_fcb(entry->fcb_index)) {
            return ERROR_INVALID_INPUT; // File is open
        }
    }

    // Free FCB and blocks
    free_fcb(entry->fcb_index);

    // Remove entry from directory
    int result = remove_entry_from_directory(current, name);
    if (result != SUCCESS) {
        return result;
    }

    // Update parent directory's modify time
    FCB* parent_fcb = get_fcb(current->fcb_index);
    if (parent_fcb != NULL) {
        update_fcb_times(parent_fcb, 0, 1, 0);
    }

    return SUCCESS;
}

