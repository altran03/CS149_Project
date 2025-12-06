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

SessionConfig *session_config;
uint8_t HARD_DISK[BLOCK_NUM][BLOCK_SIZE_BYTES];

void init_file_inode(uint16_t inode_number)
{
    Inode *in = malloc(sizeof(Inode));
    in->ownerID = session_config->uid;
    in->permissions = 420;
    in->file_size = 0;
    in->directBlocks[0] = find_free_data_block();
    for (int i = 1; i < 6; i++)
    {
        in->directBlocks[i] = 0;
    }
    in->indirect = 0;
    in->second_level_indirect = 0;
    in->time = time(NULL);
    in->ctime = time(NULL);
    in->mtime = time(NULL);
    in->dtime = 0;
    in->flags = 1;

    uint16_t inode_block = INODE_START + (inode_number / 32);
    uint16_t inode_offset = (inode_number % 32) * sizeof(Inode);

    memcpy(HARD_DISK[inode_block] + inode_offset, in, sizeof(Inode));
    free(in);
}

uint16_t create_file(const char *filename)
{
    if (filename == NULL || strlen(filename) == 0)
    {
        return 0;
    }

    if (strlen(filename) > MAX_FILENAME)
    {
        return 0;
    }

    uint16_t new_file_inode = find_free_inode();
    if (new_file_inode == 0)
    {
        return 0;
    }

    init_file_inode(new_file_inode);

    if (find_directory_entry(session_config->current_dir_inode, filename) != 0) {
        uint16_t inode_block = INODE_START + (new_file_inode / 32);
        uint16_t inode_offset = (new_file_inode % 32) * sizeof(Inode);
        Inode *temp_inode = (Inode *)(HARD_DISK[inode_block] + inode_offset);
        uint16_t data_block = temp_inode->directBlocks[0];
        free_inode(new_file_inode);
        if (data_block != 0) {
            free_data_block(data_block);
        }
        return 0;
    }

    int result = add_directory_entry(session_config->current_dir_inode, filename, new_file_inode);
    if (result != SUCCESS) {
        uint16_t inode_block = INODE_START + (new_file_inode / 32);
        uint16_t inode_offset = (new_file_inode % 32) * sizeof(Inode);
        Inode *temp_inode = (Inode *)(HARD_DISK[inode_block] + inode_offset);
        uint16_t data_block = temp_inode->directBlocks[0];
        free_inode(new_file_inode);
        if (data_block != 0) {
            free_data_block(data_block);
        }
        return 0;
    }

    return new_file_inode;
}

// Helper function: Check if operation is allowed based on inode permissions
int check_permissions(uint16_t inode_number, uint16_t operation)
{
    uint16_t inode_block = INODE_START + (inode_number / 32);
    uint16_t inode_offset = (inode_number % 32) * sizeof(Inode);
    Inode *inode = (Inode *)(HARD_DISK[inode_block] + inode_offset);
    
    uint16_t permissions = inode->permissions;
    uint16_t ownerID = inode->ownerID;
    uint16_t uid = session_config->uid;
    
    uint16_t check_bits = 0;
    if (operation & O_RDONLY || operation & O_RDWR) {
        check_bits |= 0x0100;
    }
    if (operation & O_WRONLY || operation & O_RDWR) {
        check_bits |= 0x0080;
    }
    
    if (uid == ownerID) {
        if ((permissions & check_bits) == check_bits) {
            return SUCCESS;
        }
    } else {
        uint16_t other_perms = permissions & 0x0007;
        uint16_t other_check = (check_bits >> 6) & 0x0007;
        if ((other_perms & other_check) == other_check) {
            return SUCCESS;
        }
    }
    
    return ERROR_PERMISSION_DENIED;
}

// Helper function: Traverse path and return target inode number
int traverse_path(const char *pathname, uint16_t *out_inode)
{
    if (pathname == NULL || out_inode == NULL) {
        return ERROR_INVALID_INPUT;
    }
    
    char path_copy[MAX_PATH_LENGTH];
    strncpy(path_copy, pathname, MAX_PATH_LENGTH - 1);
    path_copy[MAX_PATH_LENGTH - 1] = '\0';
    
    uint16_t current_inode;
    if (pathname[0] == '/') {
        current_inode = 0;
        if (strlen(path_copy) == 1) {
            *out_inode = 0;
            return SUCCESS;
        }
        memmove(path_copy, path_copy + 1, strlen(path_copy));
    } else {
        current_inode = session_config->current_dir_inode;
    }
    
    char *component = strtok(path_copy, "/");
    if (component == NULL) {
        *out_inode = current_inode;
        return SUCCESS;
    }
    
    while (component != NULL) {
        uint16_t found_inode = find_directory_entry(current_inode, component);
        if (found_inode == 0) {
            return ERROR_FILE_NOT_FOUND;
        }
        
        uint16_t inode_block = INODE_START + (found_inode / 32);
        uint16_t inode_offset = (found_inode % 32) * sizeof(Inode);
        Inode *inode = (Inode *)(HARD_DISK[inode_block] + inode_offset);
        
        component = strtok(NULL, "/");
        
        if (component != NULL) {
            if ((inode->flags & 2) == 0) {
                return ERROR_FILE_NOT_FOUND;
            }
            current_inode = found_inode;
        } else {
            *out_inode = found_inode;
            return SUCCESS;
        }
    }
    
    return ERROR_FILE_NOT_FOUND;
}

// Helper function: Allocate a file descriptor in the File Descriptor Table
int allocate_file_descriptor(uint16_t inode_number, uint16_t flags)
{
    uint16_t fd_per_block = BLOCK_SIZE_BYTES / sizeof(FileDescriptor);
    uint16_t total_blocks = KERNEL_MEMORY_END - KERNEL_MEMORY_START + 1;
    
    for (uint16_t block = 0; block < total_blocks; block++) {
        uint8_t *fd_block = HARD_DISK[KERNEL_MEMORY_START + block];
        
        for (uint16_t i = 0; i < fd_per_block; i++) {
            FileDescriptor *fd = (FileDescriptor *)(fd_block + i * sizeof(FileDescriptor));
            
            if (fd->inode_number == 0) {
                fd->inode_number = inode_number;
                fd->offset = 0;
                fd->flags = flags;
                fd->referenceCount = 1;
                
                uint16_t fd_index = block * fd_per_block + i;
                return (int)fd_index;
            }
        }
    }
    
    return ERROR_INVALID_INPUT;
}

// Helper function: Get file descriptor from File Descriptor Table
FileDescriptor* get_file_descriptor(uint16_t fd)
{
    uint16_t fd_per_block = BLOCK_SIZE_BYTES / sizeof(FileDescriptor);
    uint16_t total_blocks = KERNEL_MEMORY_END - KERNEL_MEMORY_START + 1;
    uint16_t max_fd = total_blocks * fd_per_block;
    
    if (fd >= max_fd) {
        return NULL;
    }
    
    uint16_t block = fd / fd_per_block;
    uint16_t index = fd % fd_per_block;
    
    uint8_t *fd_block = HARD_DISK[KERNEL_MEMORY_START + block];
    FileDescriptor *fd_ptr = (FileDescriptor *)(fd_block + index * sizeof(FileDescriptor));
    
    if (fd_ptr->inode_number == 0) {
        return NULL;
    }
    
    return fd_ptr;
}

int fs_open(const char *pathname, uint16_t operation)
{
    if (pathname == NULL) {
        return ERROR_INVALID_INPUT;
    }
    
    uint16_t target_inode;
    int result = traverse_path(pathname, &target_inode);
    if (result != SUCCESS) {
        if (result == ERROR_FILE_NOT_FOUND && (operation & O_CREAT)) {
            char path_copy[MAX_PATH_LENGTH];
            strncpy(path_copy, pathname, MAX_PATH_LENGTH - 1);
            path_copy[MAX_PATH_LENGTH - 1] = '\0';
            
            char *filename = strrchr(path_copy, '/');
            if (filename == NULL) {
                filename = path_copy;
            } else {
                filename++;
            }
            
            uint16_t created_inode = create_file(filename);
            if (created_inode == 0) {
                return ERROR_INVALID_INPUT;
            }
            
            result = traverse_path(pathname, &target_inode);
            if (result != SUCCESS) {
                return result;
            }
        } else {
            return result;
        }
    }
    
    uint16_t inode_block = INODE_START + (target_inode / 32);
    uint16_t inode_offset = (target_inode % 32) * sizeof(Inode);
    Inode *inode = (Inode *)(HARD_DISK[inode_block] + inode_offset);
    
    if ((inode->flags & 2) != 0) {
        return ERROR_INVALID_INPUT;
    }
    
    result = check_permissions(target_inode, operation);
    if (result != SUCCESS) {
        return result;
    }
    
    inode->time = time(NULL);
    memcpy(HARD_DISK[inode_block] + inode_offset, inode, sizeof(Inode));
    
    int fd = allocate_file_descriptor(target_inode, operation);
    if (fd < 0) {
        return fd;
    }
    
    return fd;
}

int fs_close(uint16_t file_descriptor)
{
    FileDescriptor *fd = get_file_descriptor(file_descriptor);
    if (fd == NULL) {
        return ERROR_INVALID_INPUT;
    }
    
    if (fd->referenceCount > 0) {
        fd->referenceCount--;
    }
    
    if (fd->referenceCount == 0) {
        fd->inode_number = 0;
        fd->offset = 0;
        fd->flags = 0;
        fd->referenceCount = 0;
    }
    
    return SUCCESS;
}

// Helper function: Recursively search directory for files matching pattern
static int search_directory_recursive(uint16_t dir_inode, const char *pattern, 
                                      char results[][MAX_PATH_LENGTH], int *result_count, 
                                      int max_results, char current_path[MAX_PATH_LENGTH])
{
    if (*result_count >= max_results) {
        return SUCCESS;
    }
    
    uint16_t inode_block = INODE_START + (dir_inode / 32);
    uint16_t inode_offset = (dir_inode % 32) * sizeof(Inode);
    Inode *dir_inode_ptr = (Inode *)(HARD_DISK[inode_block] + inode_offset);
    
    if ((dir_inode_ptr->flags & 2) == 0) {
        return ERROR_INVALID_INPUT;
    }
    
    uint16_t dir_data_block = dir_inode_ptr->directBlocks[0];
    if (dir_data_block == 0) {
        return SUCCESS;
    }
    
    uint8_t *dir_data = HARD_DISK[dir_data_block];
    uint16_t dir_size = dir_inode_ptr->file_size;
    uint16_t offset = 0;
    
    while (offset < dir_size && *result_count < max_results) {
        DirectoryEntry *entry = get_directory_entry_at_offset(dir_data, offset);
        
        if (entry->name_length > 0 && 
            !(entry->name_length == 1 && entry->name[0] == '.') &&
            !(entry->name_length == 2 && entry->name[0] == '.' && entry->name[1] == '.')) {
            
            char entry_path[MAX_PATH_LENGTH];
            char entry_name[MAX_FILENAME + 1];
            strncpy(entry_name, entry->name, entry->name_length);
            entry_name[entry->name_length] = '\0';
            
            if (strcmp(current_path, "/") == 0) {
                snprintf(entry_path, MAX_PATH_LENGTH, "/%s", entry_name);
            } else {
                snprintf(entry_path, MAX_PATH_LENGTH, "%s/%s", current_path, entry_name);
            }
            
            if (strstr(entry->name, pattern) != NULL) {
                uint16_t entry_inode = entry->inode_number;
                uint16_t entry_inode_block = INODE_START + (entry_inode / 32);
                uint16_t entry_inode_offset = (entry_inode % 32) * sizeof(Inode);
                Inode *entry_inode_ptr = (Inode *)(HARD_DISK[entry_inode_block] + entry_inode_offset);
                
                if ((entry_inode_ptr->flags & 2) == 0) {
                    strncpy(results[*result_count], entry_path, MAX_PATH_LENGTH - 1);
                    results[*result_count][MAX_PATH_LENGTH - 1] = '\0';
                    (*result_count)++;
                }
            }
            
            uint16_t entry_inode = entry->inode_number;
            uint16_t entry_inode_block = INODE_START + (entry_inode / 32);
            uint16_t entry_inode_offset = (entry_inode % 32) * sizeof(Inode);
            Inode *entry_inode_ptr = (Inode *)(HARD_DISK[entry_inode_block] + entry_inode_offset);
            
            if ((entry_inode_ptr->flags & 2) != 0) {
                search_directory_recursive(entry_inode, pattern, results, result_count, 
                                          max_results, entry_path);
            }
        }
        
        offset = get_next_directory_entry_offset(dir_data, offset, dir_size);
    }
    
    return SUCCESS;
}

int search_files_by_name(const char *search_path, const char *pattern, 
                         char results[][MAX_PATH_LENGTH], int max_results)
{
    if (search_path == NULL || pattern == NULL || results == NULL || max_results <= 0) {
        return ERROR_INVALID_INPUT;
    }
    
    uint16_t start_inode;
    int result = traverse_path(search_path, &start_inode);
    if (result != SUCCESS) {
        return result;
    }
    
    uint16_t inode_block = INODE_START + (start_inode / 32);
    uint16_t inode_offset = (start_inode % 32) * sizeof(Inode);
    Inode *inode = (Inode *)(HARD_DISK[inode_block] + inode_offset);
    
    if ((inode->flags & 2) == 0) {
        return ERROR_INVALID_INPUT;
    }
    
    int result_count = 0;
    char current_path[MAX_PATH_LENGTH];
    strncpy(current_path, search_path, MAX_PATH_LENGTH - 1);
    current_path[MAX_PATH_LENGTH - 1] = '\0';
    
    size_t len = strlen(current_path);
    if (len > 1 && current_path[len - 1] == '/') {
        current_path[len - 1] = '\0';
    }
    
    result = search_directory_recursive(start_inode, pattern, results, &result_count, 
                                       max_results, current_path);
    
    if (result != SUCCESS) {
        return result;
    }
    
    return result_count;
}

int fs_read(uint16_t file_descriptor, void *buffer, size_t count)
{
    if (buffer == NULL) {
        return ERROR_INVALID_INPUT;
    }
    
    FileDescriptor *fd = get_file_descriptor(file_descriptor);
    if (fd == NULL) {
        return ERROR_INVALID_INPUT;
    }
    
    if ((fd->flags & O_RDONLY) == 0 && (fd->flags & O_RDWR) == 0) {
        return ERROR_PERMISSION_DENIED;
    }
    
    uint16_t inode_number = fd->inode_number;
    uint16_t inode_block = INODE_START + (inode_number / 32);
    uint16_t inode_offset = (inode_number % 32) * sizeof(Inode);
    Inode *inode = (Inode *)(HARD_DISK[inode_block] + inode_offset);
    
    if ((inode->flags & 2) != 0) {
        return ERROR_INVALID_INPUT;
    }
    
    uint32_t file_size = inode->file_size;
    uint16_t current_offset = fd->offset;
    
    if (current_offset >= file_size) {
        return 0;
    }
    
    size_t bytes_to_read = count;
    if (current_offset + bytes_to_read > file_size) {
        bytes_to_read = file_size - current_offset;
    }
    
    size_t bytes_read = 0;
    uint16_t block_index = current_offset / BLOCK_SIZE_BYTES;
    uint16_t offset_in_block = current_offset % BLOCK_SIZE_BYTES;
    
    while (bytes_read < bytes_to_read && block_index < 6) {
        uint16_t data_block = inode->directBlocks[block_index];
        if (data_block == 0) {
            break;
        }
        
        size_t bytes_from_block = BLOCK_SIZE_BYTES - offset_in_block;
        if (bytes_read + bytes_from_block > bytes_to_read) {
            bytes_from_block = bytes_to_read - bytes_read;
        }
        
        memcpy((uint8_t *)buffer + bytes_read, 
               HARD_DISK[data_block] + offset_in_block, 
               bytes_from_block);
        
        bytes_read += bytes_from_block;
        block_index++;
        offset_in_block = 0;
    }
    
    fd->offset += bytes_read;
    
    inode->time = time(NULL);
    memcpy(HARD_DISK[inode_block] + inode_offset, inode, sizeof(Inode));
    
    return (int)bytes_read;
}

int fs_write(uint16_t file_descriptor, const void *buffer, size_t count)
{
    if (buffer == NULL) {
        return ERROR_INVALID_INPUT;
    }
    
    FileDescriptor *fd = get_file_descriptor(file_descriptor);
    if (fd == NULL) {
        return ERROR_INVALID_INPUT;
    }
    
    if ((fd->flags & O_WRONLY) == 0 && (fd->flags & O_RDWR) == 0) {
        return ERROR_PERMISSION_DENIED;
    }
    
    uint16_t inode_number = fd->inode_number;
    uint16_t inode_block = INODE_START + (inode_number / 32);
    uint16_t inode_offset = (inode_number % 32) * sizeof(Inode);
    Inode *inode = (Inode *)(HARD_DISK[inode_block] + inode_offset);
    
    if ((inode->flags & 2) != 0) {
        return ERROR_INVALID_INPUT;
    }
    
    size_t bytes_written = 0;
    uint16_t current_offset = fd->offset;
    uint16_t block_index = current_offset / BLOCK_SIZE_BYTES;
    uint16_t offset_in_block = current_offset % BLOCK_SIZE_BYTES;
    
    while (bytes_written < count && block_index < 6) {
        uint16_t data_block = inode->directBlocks[block_index];
        if (data_block == 0) {
            data_block = find_free_data_block();
            if (data_block == 0) {
                break;
            }
            inode->directBlocks[block_index] = data_block;
        }
        
        size_t bytes_to_block = BLOCK_SIZE_BYTES - offset_in_block;
        if (bytes_written + bytes_to_block > count) {
            bytes_to_block = count - bytes_written;
        }
        
        memcpy(HARD_DISK[data_block] + offset_in_block,
               (const uint8_t *)buffer + bytes_written,
               bytes_to_block);
        
        bytes_written += bytes_to_block;
        block_index++;
        offset_in_block = 0;
    }
    
    uint32_t new_size = current_offset + bytes_written;
    if (new_size > inode->file_size) {
        inode->file_size = new_size;
    }
    
    fd->offset += bytes_written;
    
    inode->mtime = time(NULL);
    inode->time = time(NULL);
    
    memcpy(HARD_DISK[inode_block] + inode_offset, inode, sizeof(Inode));
    
    return (int)bytes_written;
}

void reset_hard_disk()
{
    memset(HARD_DISK, 0, sizeof(HARD_DISK));
}
