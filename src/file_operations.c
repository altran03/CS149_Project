#include "headers/common.h"
#include "headers/utils.h"
#include "headers/file_operations.h"
#include "headers/directory_operations.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

// Global session configuration
SessionConfig *session_config;

// HARD DISK - actual storage array
// 16 bits is enough for number of blocks 2^16=65536>16384, each block is 2KiB
uint8_t HARD_DISK[BLOCK_NUM][BLOCK_SIZE_BYTES];

// should not be called by itself, already called in create_file()
void init_file_inode(uint16_t inode_number)
{
    Inode *in = malloc(sizeof(Inode));
    in->ownerID = session_config->uid;            // set creator/owner to current session user
    in->permissions = 420;                        // 0000000rw-r--r--, owner, group, other/world i.e. 4+32+128+256=420
    in->file_size = 0;                            // in bytes, empty file initially
    in->directBlocks[0] = find_free_data_block(); // initialize first block
    for (int i = 1; i < 6; i++)
    {
        in->directBlocks[i] = 0; // initialize remaining direct blocks to 0
    }
    in->indirect = 0;
    in->second_level_indirect = 0;
    in->time = time(NULL);  // last accessed, since unix epoch
    in->ctime = time(NULL); // creation time
    in->mtime = time(NULL); // last modified
    in->dtime = 0;          // file deletion time, not set
    in->flags = 1;          // 1 regular file (not directory)

    // Calculate which block contains this inode and offset within that block
    // Each inode is 64 bytes, each block is 2048 bytes, so 32 inodes per block
    uint16_t inode_block = INODE_START + (inode_number / 32);
    uint16_t inode_offset = (inode_number % 32) * sizeof(Inode);

    memcpy(HARD_DISK[inode_block] + inode_offset, in, sizeof(Inode));
    free(in);
}

uint16_t create_file(const char *filename)
{
    // Validate input
    if (filename == NULL || strlen(filename) == 0)
    {
        return 0;
    }

    if (strlen(filename) > MAX_FILENAME)
    {
        return 0;
    }

    // Find a free inode for the new file
    uint16_t new_file_inode = find_free_inode();
    if (new_file_inode == 0)
    {
        // Assuming find_free_inode returns 0 when no free inode is found
        return 0; // No free inodes
    }

    // Initialize the inode for the file
    init_file_inode(new_file_inode);

    // Create File struct (similar to Directory)
    File *file = malloc(sizeof(File));
    if (file == NULL)
    {
        free_inode(new_file_inode);
        return 0; // Memory allocation failed
    }
    file->inode_number = new_file_inode;
    file->file_size = 0;

    // Allocate and set the path
    size_t path_len = strlen(session_config->current_working_dir) + strlen(filename) + 2; // 2 for / and \0
    file->path = malloc(path_len);
    if (file->path == NULL)
    {
        free(file);
        free_inode(new_file_inode);
        return 0; // Memory allocation failed
    }

    if (snprintf(file->path, path_len, "%s/%s", session_config->current_working_dir, filename) >= (int)path_len)
    {
        printf("MAX_PATH_LENGTH exceeded\n");
        free(file->path);
        free(file);
        free_inode(new_file_inode);
        return 0;
    }

    // Check if file already exists in current directory
    if (find_directory_entry(session_config->current_dir_inode, filename) != 0) {
        // File already exists, clean up and return error
        // Get the inode to find the data block before freeing
        uint16_t inode_block = INODE_START + (new_file_inode / 32);
        uint16_t inode_offset = (new_file_inode % 32) * sizeof(Inode);
        Inode *temp_inode = (Inode *)(HARD_DISK[inode_block] + inode_offset);
        uint16_t data_block = temp_inode->directBlocks[0];
        free_inode(new_file_inode); // Free the inode we just allocated
        if (data_block != 0) {
            free_data_block(data_block);
        }
        free(file->path);
        free(file);
        return 0; // File already exists
    }

    // Add DirectoryEntry to current directory
    int result = add_directory_entry(session_config->current_dir_inode, filename, new_file_inode);
    if (result != SUCCESS) {
        // Failed to add directory entry, clean up
        // Get the inode to find the data block before freeing
        uint16_t inode_block = INODE_START + (new_file_inode / 32);
        uint16_t inode_offset = (new_file_inode % 32) * sizeof(Inode);
        Inode *temp_inode = (Inode *)(HARD_DISK[inode_block] + inode_offset);
        uint16_t data_block = temp_inode->directBlocks[0];
        free_inode(new_file_inode);
        if (data_block != 0) {
            free_data_block(data_block);
        }
        free(file->path);
        free(file);
        return 0; // Return 0 on error
    }

    // Note: File struct should be managed by the caller
    // or stored in a file table, freeing it for now
    free(file->path);
    free(file);

    return new_file_inode; // Return inode number on success
}

int delete_file(const char *filename)
{
    // TODO: Implement file deletion
    return SUCCESS;
}

// Helper function: Check if operation is allowed based on inode permissions
// Returns SUCCESS if allowed, ERROR_PERMISSION_DENIED if not
int check_permissions(uint16_t inode_number, uint16_t operation)
{
    // Get the inode
    uint16_t inode_block = INODE_START + (inode_number / 32);
    uint16_t inode_offset = (inode_number % 32) * sizeof(Inode);
    Inode *inode = (Inode *)(HARD_DISK[inode_block] + inode_offset);
    
    uint16_t permissions = inode->permissions;
    uint16_t ownerID = inode->ownerID;
    uint16_t uid = session_config->uid;
    
    // Determine which permission bits to check
    uint16_t check_bits = 0;
    if (operation & O_RDONLY || operation & O_RDWR) {
        check_bits |= 0x0100; // Read bit (bit 8)
    }
    if (operation & O_WRONLY || operation & O_RDWR) {
        check_bits |= 0x0080; // Write bit (bit 7)
    }
    
    // Check permissions: owner (bits 8-6), group (bits 5-3), other (bits 2-0)
    // Format: 0000000rwxrwxrwx
    if (uid == ownerID) {
        // Check owner permissions (bits 8-6: rwx)
        if ((permissions & check_bits) == check_bits) {
            return SUCCESS;
        }
    } else {
        // For simplicity, check other permissions (bits 2-0: rwx)
        // In a full implementation, we'd check group membership
        uint16_t other_perms = permissions & 0x0007; // bits 2-0
        uint16_t other_check = (check_bits >> 6) & 0x0007; // Shift to other position
        if ((other_perms & other_check) == other_check) {
            return SUCCESS;
        }
    }
    
    return ERROR_PERMISSION_DENIED;
}

// Helper function: Traverse path and return target inode number
// Returns SUCCESS and sets out_inode if found, ERROR_FILE_NOT_FOUND otherwise
int traverse_path(const char *pathname, uint16_t *out_inode)
{
    if (pathname == NULL || out_inode == NULL) {
        return ERROR_INVALID_INPUT;
    }
    
    // Copy pathname since strtok modifies it
    char path_copy[MAX_PATH_LENGTH];
    strncpy(path_copy, pathname, MAX_PATH_LENGTH - 1);
    path_copy[MAX_PATH_LENGTH - 1] = '\0';
    
    // Start from root if absolute path, or current directory if relative
    uint16_t current_inode;
    if (pathname[0] == '/') {
        current_inode = 0; // Root inode
        // Path is just "/"
        if (strlen(path_copy) == 1) {
            *out_inode = 0;
            return SUCCESS;
        }
        // Move past leading slash for tokenization
        memmove(path_copy, path_copy + 1, strlen(path_copy));
    } else {
        current_inode = session_config->current_dir_inode;
    }
    
    // Tokenize path and traverse
    char *component = strtok(path_copy, "/");
    if (component == NULL) {
        // Empty path after tokenization
        *out_inode = current_inode;
        return SUCCESS;
    }
    
    while (component != NULL) {
        // Find the component in current directory
        uint16_t found_inode = find_directory_entry(current_inode, component);
        if (found_inode == 0) {
            return ERROR_FILE_NOT_FOUND;
        }
        
        // Check if it's a directory (for intermediate components)
        uint16_t inode_block = INODE_START + (found_inode / 32);
        uint16_t inode_offset = (found_inode % 32) * sizeof(Inode);
        Inode *inode = (Inode *)(HARD_DISK[inode_block] + inode_offset);
        
        // Get next component
        component = strtok(NULL, "/");
        
        if (component != NULL) {
            // More components to traverse - must be a directory
            if ((inode->flags & 2) == 0) {
                return ERROR_FILE_NOT_FOUND; // Not a directory
            }
            current_inode = found_inode;
        } else {
            // Last component - this is our target
            *out_inode = found_inode;
            return SUCCESS;
        }
    }
    
    return ERROR_FILE_NOT_FOUND;
}

// Helper function: Allocate a file descriptor in the File Descriptor Table
// Returns file descriptor index (>= 0) on success, negative error code on failure
int allocate_file_descriptor(uint16_t inode_number, uint16_t flags)
{
    // FileDescriptor is 64 bytes, each block is 2048 bytes
    // So 32 file descriptors per block (2048 / 64 = 32)
    // Total blocks: KERNEL_MEMORY_END - KERNEL_MEMORY_START + 1 = 17 blocks
    // Total capacity: 17 * 32 = 544 file descriptors
    
    uint16_t fd_per_block = BLOCK_SIZE_BYTES / sizeof(FileDescriptor); // 32
    uint16_t total_blocks = KERNEL_MEMORY_END - KERNEL_MEMORY_START + 1; // 17
    
    // Search for a free file descriptor slot
    for (uint16_t block = 0; block < total_blocks; block++) {
        uint8_t *fd_block = HARD_DISK[KERNEL_MEMORY_START + block];
        
        for (uint16_t i = 0; i < fd_per_block; i++) {
            FileDescriptor *fd = (FileDescriptor *)(fd_block + i * sizeof(FileDescriptor));
            
            // Check if slot is free (inode_number == 0 means free)
            if (fd->inode_number == 0) {
                // Allocate this slot
                fd->inode_number = inode_number;
                fd->offset = 0;
                fd->flags = flags;
                fd->referenceCount = 1;
                
                // Calculate and return file descriptor index
                uint16_t fd_index = block * fd_per_block + i;
                return (int)fd_index;
            }
        }
    }
    
    return ERROR_INVALID_INPUT; // No free file descriptors
}

// Helper function: Get file descriptor from File Descriptor Table
// Returns pointer to FileDescriptor or NULL if invalid
FileDescriptor* get_file_descriptor(uint16_t fd)
{
    uint16_t fd_per_block = BLOCK_SIZE_BYTES / sizeof(FileDescriptor); // 32
    uint16_t total_blocks = KERNEL_MEMORY_END - KERNEL_MEMORY_START + 1; // 17
    uint16_t max_fd = total_blocks * fd_per_block; // 544
    
    if (fd >= max_fd) {
        return NULL; // Invalid file descriptor
    }
    
    uint16_t block = fd / fd_per_block;
    uint16_t index = fd % fd_per_block;
    
    uint8_t *fd_block = HARD_DISK[KERNEL_MEMORY_START + block];
    FileDescriptor *fd_ptr = (FileDescriptor *)(fd_block + index * sizeof(FileDescriptor));
    
    // Check if it's actually allocated
    if (fd_ptr->inode_number == 0) {
        return NULL; // Not allocated
    }
    
    return fd_ptr;
}

// assuming /foo/bar is pathname and op is O_RDONLY
int fs_open(const char *pathname, uint16_t operation)
{
    if (pathname == NULL) {
        return ERROR_INVALID_INPUT;
    }
    
    // Traverse path to find the target file/directory
    uint16_t target_inode;
    int result = traverse_path(pathname, &target_inode);
    if (result != SUCCESS) {
        // If file doesn't exist and O_CREAT is set, create it
        if (result == ERROR_FILE_NOT_FOUND && (operation & O_CREAT)) {
            // Extract filename from path
            char path_copy[MAX_PATH_LENGTH];
            strncpy(path_copy, pathname, MAX_PATH_LENGTH - 1);
            path_copy[MAX_PATH_LENGTH - 1] = '\0';
            
            char *filename = strrchr(path_copy, '/');
            if (filename == NULL) {
                filename = path_copy;
            } else {
                filename++; // Skip the '/'
            }
            
            // Create the file
            uint16_t created_inode = create_file(filename);
            if (created_inode == 0) {
                return ERROR_INVALID_INPUT; // Failed to create file
            }
            
            // Traverse again to get the newly created file's inode
            result = traverse_path(pathname, &target_inode);
            if (result != SUCCESS) {
                return result;
            }
        } else {
            return result;
        }
    }
    
    // Check if target is a file (not a directory)
    uint16_t inode_block = INODE_START + (target_inode / 32);
    uint16_t inode_offset = (target_inode % 32) * sizeof(Inode);
    Inode *inode = (Inode *)(HARD_DISK[inode_block] + inode_offset);
    
    if ((inode->flags & 2) != 0) {
        return ERROR_INVALID_INPUT; // Cannot open directory as file
    }
    
    // Check permissions
    result = check_permissions(target_inode, operation);
    if (result != SUCCESS) {
        return result;
    }
    
    // Update last accessed time
    inode->time = time(NULL);
    memcpy(HARD_DISK[inode_block] + inode_offset, inode, sizeof(Inode));
    
    // Allocate file descriptor
    int fd = allocate_file_descriptor(target_inode, operation);
    if (fd < 0) {
        return fd; // Error allocating file descriptor
    }
    
    return fd; // Return file descriptor index
}

int fs_close(uint16_t file_descriptor)
{
    // Get the file descriptor
    FileDescriptor *fd = get_file_descriptor(file_descriptor);
    if (fd == NULL) {
        return ERROR_INVALID_INPUT; // Invalid file descriptor
    }
    
    // Decrement reference count
    if (fd->referenceCount > 0) {
        fd->referenceCount--;
    }
    
    // If reference count reaches 0, free the file descriptor
    if (fd->referenceCount == 0) {
        // Clear the file descriptor (mark as free)
        fd->inode_number = 0;
        fd->offset = 0;
        fd->flags = 0;
        fd->referenceCount = 0;
    }
    
    return SUCCESS;
}

// Helper function: Recursively search directory for files matching pattern
// This is a helper for search_files_by_name
static int search_directory_recursive(uint16_t dir_inode, const char *pattern, 
                                      char results[][MAX_PATH_LENGTH], int *result_count, 
                                      int max_results, char current_path[MAX_PATH_LENGTH])
{
    if (*result_count >= max_results) {
        return SUCCESS; // Reached max results
    }
    
    // Get the directory's inode
    uint16_t inode_block = INODE_START + (dir_inode / 32);
    uint16_t inode_offset = (dir_inode % 32) * sizeof(Inode);
    Inode *dir_inode_ptr = (Inode *)(HARD_DISK[inode_block] + inode_offset);
    
    // Check if it's actually a directory
    if ((dir_inode_ptr->flags & 2) == 0) {
        return ERROR_INVALID_INPUT; // Not a directory
    }
    
    // Get the directory's first data block
    uint16_t dir_data_block = dir_inode_ptr->directBlocks[0];
    if (dir_data_block == 0) {
        return SUCCESS; // Empty directory
    }
    
    uint8_t *dir_data = HARD_DISK[dir_data_block];
    uint16_t dir_size = dir_inode_ptr->file_size;
    uint16_t offset = 0;
    
    // Iterate through directory entries
    while (offset < dir_size && *result_count < max_results) {
        DirectoryEntry *entry = get_directory_entry_at_offset(dir_data, offset);
        
        // Skip . and .. entries
        if (entry->name_length > 0 && 
            !(entry->name_length == 1 && entry->name[0] == '.') &&
            !(entry->name_length == 2 && entry->name[0] == '.' && entry->name[1] == '.')) {
            
            // Build full path for this entry
            char entry_path[MAX_PATH_LENGTH];
            char entry_name[MAX_FILENAME + 1];
            strncpy(entry_name, entry->name, entry->name_length);
            entry_name[entry->name_length] = '\0';
            
            if (strcmp(current_path, "/") == 0) {
                // Root directory - just add name
                snprintf(entry_path, MAX_PATH_LENGTH, "/%s", entry_name);
            } else {
                // Normal directory - add slash and name
                snprintf(entry_path, MAX_PATH_LENGTH, "%s/%s", current_path, entry_name);
            }
            
            // Check if name matches pattern (simple substring match)
            // For more advanced matching, could use fnmatch or regex
            if (strstr(entry->name, pattern) != NULL) {
                // Get the entry's inode to check if it's a file
                uint16_t entry_inode = entry->inode_number;
                uint16_t entry_inode_block = INODE_START + (entry_inode / 32);
                uint16_t entry_inode_offset = (entry_inode % 32) * sizeof(Inode);
                Inode *entry_inode_ptr = (Inode *)(HARD_DISK[entry_inode_block] + entry_inode_offset);
                
                // Only add files (not directories) to results
                if ((entry_inode_ptr->flags & 2) == 0) {
                    // It's a file, add to results
                    strncpy(results[*result_count], entry_path, MAX_PATH_LENGTH - 1);
                    results[*result_count][MAX_PATH_LENGTH - 1] = '\0';
                    (*result_count)++;
                }
            }
            
            // If it's a directory, recursively search it
            uint16_t entry_inode = entry->inode_number;
            uint16_t entry_inode_block = INODE_START + (entry_inode / 32);
            uint16_t entry_inode_offset = (entry_inode % 32) * sizeof(Inode);
            Inode *entry_inode_ptr = (Inode *)(HARD_DISK[entry_inode_block] + entry_inode_offset);
            
            if ((entry_inode_ptr->flags & 2) != 0) {
                // It's a directory, recursively search it
                search_directory_recursive(entry_inode, pattern, results, result_count, 
                                          max_results, entry_path);
            }
        }
        
        // Move to next entry
        offset = get_next_directory_entry_offset(dir_data, offset, dir_size);
    }
    
    return SUCCESS;
}

// Search for files by name/path pattern
// Returns number of matches found, or negative error code
int search_files_by_name(const char *search_path, const char *pattern, 
                         char results[][MAX_PATH_LENGTH], int max_results)
{
    if (search_path == NULL || pattern == NULL || results == NULL || max_results <= 0) {
        return ERROR_INVALID_INPUT;
    }
    
    // Find the starting directory inode
    uint16_t start_inode;
    int result = traverse_path(search_path, &start_inode);
    if (result != SUCCESS) {
        return result;
    }
    
    // Verify it's a directory
    uint16_t inode_block = INODE_START + (start_inode / 32);
    uint16_t inode_offset = (start_inode % 32) * sizeof(Inode);
    Inode *inode = (Inode *)(HARD_DISK[inode_block] + inode_offset);
    
    if ((inode->flags & 2) == 0) {
        return ERROR_INVALID_INPUT; // Not a directory
    }
    
    // Initialize results
    int result_count = 0;
    char current_path[MAX_PATH_LENGTH];
    strncpy(current_path, search_path, MAX_PATH_LENGTH - 1);
    current_path[MAX_PATH_LENGTH - 1] = '\0';
    
    // Normalize path - remove trailing slash unless it's root
    size_t len = strlen(current_path);
    if (len > 1 && current_path[len - 1] == '/') {
        current_path[len - 1] = '\0';
    }
    
    // Recursively search
    result = search_directory_recursive(start_inode, pattern, results, &result_count, 
                                       max_results, current_path);
    
    if (result != SUCCESS) {
        return result;
    }
    
    return result_count; // Return number of matches found
}

// Read data from a file
// Returns number of bytes read, or negative error code
int fs_read(uint16_t file_descriptor, void *buffer, size_t count)
{
    if (buffer == NULL) {
        return ERROR_INVALID_INPUT;
    }
    
    // Get the file descriptor
    FileDescriptor *fd = get_file_descriptor(file_descriptor);
    if (fd == NULL) {
        return ERROR_INVALID_INPUT; // Invalid file descriptor
    }
    
    // Check if read is allowed
    if ((fd->flags & O_RDONLY) == 0 && (fd->flags & O_RDWR) == 0) {
        return ERROR_PERMISSION_DENIED;
    }
    
    // Get the file's inode
    uint16_t inode_number = fd->inode_number;
    uint16_t inode_block = INODE_START + (inode_number / 32);
    uint16_t inode_offset = (inode_number % 32) * sizeof(Inode);
    Inode *inode = (Inode *)(HARD_DISK[inode_block] + inode_offset);
    
    // Check if it's a file (not a directory)
    if ((inode->flags & 2) != 0) {
        return ERROR_INVALID_INPUT; // Cannot read directory
    }
    
    // Calculate how much we can read
    uint32_t file_size = inode->file_size;
    uint16_t current_offset = fd->offset;
    
    if (current_offset >= file_size) {
        return 0; // Already at end of file
    }
    
    // Calculate how many bytes to read
    size_t bytes_to_read = count;
    if (current_offset + bytes_to_read > file_size) {
        bytes_to_read = file_size - current_offset;
    }
    
    // Read from direct blocks
    size_t bytes_read = 0;
    uint16_t block_index = current_offset / BLOCK_SIZE_BYTES;
    uint16_t offset_in_block = current_offset % BLOCK_SIZE_BYTES;
    
    while (bytes_read < bytes_to_read && block_index < 6) {
        uint16_t data_block = inode->directBlocks[block_index];
        if (data_block == 0) {
            break; // No more blocks
        }
        
        // Calculate how much to read from this block
        size_t bytes_from_block = BLOCK_SIZE_BYTES - offset_in_block;
        if (bytes_read + bytes_from_block > bytes_to_read) {
            bytes_from_block = bytes_to_read - bytes_read;
        }
        
        // Copy data from block to buffer
        memcpy((uint8_t *)buffer + bytes_read, 
               HARD_DISK[data_block] + offset_in_block, 
               bytes_from_block);
        
        bytes_read += bytes_from_block;
        block_index++;
        offset_in_block = 0; // Next block starts at beginning
    }
    
    // Update file descriptor offset
    fd->offset += bytes_read;
    
    // Update last accessed time in inode
    inode->time = time(NULL);
    memcpy(HARD_DISK[inode_block] + inode_offset, inode, sizeof(Inode));
    
    return (int)bytes_read;
}

// Write data to a file
// Returns number of bytes written, or negative error code
int fs_write(uint16_t file_descriptor, const void *buffer, size_t count)
{
    if (buffer == NULL) {
        return ERROR_INVALID_INPUT;
    }
    
    // Get the file descriptor
    FileDescriptor *fd = get_file_descriptor(file_descriptor);
    if (fd == NULL) {
        return ERROR_INVALID_INPUT; // Invalid file descriptor
    }
    
    // Check if write is allowed
    if ((fd->flags & O_WRONLY) == 0 && (fd->flags & O_RDWR) == 0) {
        return ERROR_PERMISSION_DENIED;
    }
    
    // Get the file's inode
    uint16_t inode_number = fd->inode_number;
    uint16_t inode_block = INODE_START + (inode_number / 32);
    uint16_t inode_offset = (inode_number % 32) * sizeof(Inode);
    Inode *inode = (Inode *)(HARD_DISK[inode_block] + inode_offset);
    
    // Check if it's a file (not a directory)
    if ((inode->flags & 2) != 0) {
        return ERROR_INVALID_INPUT; // Cannot write to directory
    }
    
    // Write to direct blocks
    size_t bytes_written = 0;
    uint16_t current_offset = fd->offset;
    uint16_t block_index = current_offset / BLOCK_SIZE_BYTES;
    uint16_t offset_in_block = current_offset % BLOCK_SIZE_BYTES;
    
    while (bytes_written < count && block_index < 6) {
        // Get or allocate data block
        uint16_t data_block = inode->directBlocks[block_index];
        if (data_block == 0) {
            // Need to allocate a new block
            data_block = find_free_data_block();
            if (data_block == 0) {
                break; // No free blocks available
            }
            inode->directBlocks[block_index] = data_block;
        }
        
        // Calculate how much to write to this block
        size_t bytes_to_block = BLOCK_SIZE_BYTES - offset_in_block;
        if (bytes_written + bytes_to_block > count) {
            bytes_to_block = count - bytes_written;
        }
        
        // Copy data from buffer to block
        memcpy(HARD_DISK[data_block] + offset_in_block,
               (const uint8_t *)buffer + bytes_written,
               bytes_to_block);
        
        bytes_written += bytes_to_block;
        block_index++;
        offset_in_block = 0; // Next block starts at beginning
    }
    
    // Update file size if we wrote past the end
    uint32_t new_size = current_offset + bytes_written;
    if (new_size > inode->file_size) {
        inode->file_size = new_size;
    }
    
    // Update file descriptor offset
    fd->offset += bytes_written;
    
    // Update modification and access times
    inode->mtime = time(NULL);
    inode->time = time(NULL);
    
    // Write back updated inode
    memcpy(HARD_DISK[inode_block] + inode_offset, inode, sizeof(Inode));
    
    return (int)bytes_written;
}

// creating the free bit map/array

// reset_HARD_DISK fills hard disk with 0s
void reset_hard_disk()
{
    memset(HARD_DISK, 0, sizeof(HARD_DISK));
}

// print contents of hard disk in ascii, from starting block (inclusive) to end_block (exclusive/non-inclusive)
int print_hard_disk_ascii(int start_block, int end_block)
{
    if (start_block < 0 || end_block < 0 || start_block > BLOCK_NUM - 1 || end_block > BLOCK_NUM)
    {
        printf("Bounds of blocks should be between 0 and BLOCK_NUM, start: %d, end: %d\n", start_block, end_block);
        return 1;
    }
    for (int i = start_block; i < end_block; i++)
    {
        for (int j = 0; j < BLOCK_SIZE_BYTES; j++)
        {
            // check for printable character
            if (HARD_DISK[i][j] >= 32 && HARD_DISK[i][j] <= 126)
                printf("%c", HARD_DISK[i][j]);
            else
                printf(".");
        }
        printf("\n");
    }
    return 0;
}

