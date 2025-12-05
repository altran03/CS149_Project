#include "headers/common.h"
#include "headers/utils.h"
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

// External references to globals defined in file_operations.c
extern SessionConfig *session_config;
extern uint8_t HARD_DISK[BLOCK_NUM][BLOCK_SIZE_BYTES];

int init_inodes(const char *filename)
{
    // TODO: Implement inode initialization from file
    return SUCCESS;
}

// should not be called by itself, already called in create_root_directory()
void init_root_inode()
{
    Inode *in = malloc(sizeof(Inode));
    in->ownerID = session_config->uid;    // set creator/owner to current session user
    in->permissions = 420;                // 0000000rw-r--r--, owner, group, other/world i.e. 4+32+128+256=420
    in->file_size = 0;                    // in bytes
    in->directBlocks[0] = ROOT_DIRECTORY; // initialize first block
    in->indirect = 0;
    in->second_level_indirect = 0;
    in->time = time(NULL);  // last accessed, since unix epoch
    in->ctime = time(NULL); // creation time
    in->mtime = time(NULL); // last modified
    in->dtime = 0;          // file deletion time, not set
    in->flags = 2;          // 2 directory
    memcpy(HARD_DISK[INODE_START], in, sizeof(Inode));//saved to hard disk array
}

// Creates root directory: initializes root inode and root directory data block
// Root directory data block contains: . (self) and .. (parent, also self for root)
void create_root_directory(){
    // Initialize root inode (inode 0 at INODE_START block)
    init_root_inode();
    
    // Initialize root directory data block with . and .. entries
    uint8_t *root_data_block = HARD_DISK[ROOT_DIRECTORY];
    uint16_t offset = 0;
    
    // Create . entry (points to root inode)
    DirectoryEntry *dot_entry = (DirectoryEntry *)(root_data_block + offset);
    dot_entry->inode_number = 0; // Root inode number (stored at block INODE_START)
    dot_entry->name_length = 1;
    dot_entry->name[0] = '.';
    dot_entry->name[1] = '\0';
    dot_entry->record_length = sizeof(DirectoryEntry) + 2; // 2 bytes for "." + null terminator
    offset += dot_entry->record_length;
    
    // Create .. entry (also points to root inode since root is its own parent)
    DirectoryEntry *dotdot_entry = (DirectoryEntry *)(root_data_block + offset);
    dotdot_entry->inode_number = 0; // Root inode number (root is its own parent)
    dotdot_entry->name_length = 2;
    dotdot_entry->name[0] = '.';
    dotdot_entry->name[1] = '.';
    dotdot_entry->name[2] = '\0';
    dotdot_entry->record_length = sizeof(DirectoryEntry) + 3; // 3 bytes for ".." + null terminator
    
    // Update root inode file_size to reflect the directory entries
    Inode *root_inode = (Inode *)HARD_DISK[INODE_START];
    root_inode->file_size = dot_entry->record_length + dotdot_entry->record_length;
}

// find_free_data_block() and find_free_inode() are now implemented in utils.c

// should not be called by itself, already called in create_directory()
void init_inode(uint16_t inode_number)
{
    Inode *in = malloc(sizeof(Inode));
    in->ownerID = session_config->uid;            // set creator/owner to current session user
    in->permissions = 420;                        // 0000000rw-r--r--, owner, group, other/world i.e. 4+32+128+256=420
    in->file_size = 0;                            // in bytes
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
    in->flags = 2;          // 2 directory

    // Calculate which block contains this inode and offset within that block
    // Each inode is 64 bytes, each block is 2048 bytes, so 32 inodes per block
    uint16_t inode_block = INODE_START + (inode_number / 32);
    uint16_t inode_offset = (inode_number % 32) * sizeof(Inode);

    memcpy(HARD_DISK[inode_block] + inode_offset, in, sizeof(Inode));
    free(in);
}

// Creates a new directory with given name in current working directory
// Returns inode number of created directory, or 0 on error
uint16_t create_directory(const char *dirname)
{
    // Validate input
    if (dirname == NULL || strlen(dirname) == 0 || strlen(dirname) > MAX_FILENAME) {
        printf("INVALID dirname: %s\n", dirname);
        return 0;
    }
    
    // Find free inode for new directory
    uint16_t new_inode_num = find_free_inode();
    if (new_inode_num == 0) {
        printf("No free Inodes\n");
        return 0; // No free inodes
    }
    
    // Initialize the directory's inode
    init_inode(new_inode_num);
    
    // Get the directory's data block
    Inode *dir_inode = (Inode *)(HARD_DISK[INODE_START + (new_inode_num / 32)] + (new_inode_num % 32) * sizeof(Inode));
    uint16_t dir_data_block = dir_inode->directBlocks[0];
    uint8_t *dir_data = HARD_DISK[dir_data_block];
    uint16_t offset = 0;
    
    // Create . entry (points to this directory's inode)
    DirectoryEntry *dot_entry = (DirectoryEntry *)(dir_data + offset);
    dot_entry->inode_number = new_inode_num;
    dot_entry->name_length = 1;
    dot_entry->name[0] = '.';
    dot_entry->name[1] = '\0';
    dot_entry->record_length = sizeof(DirectoryEntry) + 2;
    offset += dot_entry->record_length;
    
    // Create .. entry (points to parent directory's inode)
    // Use current_dir_inode from session_config as the parent
    DirectoryEntry *dotdot_entry = (DirectoryEntry *)(dir_data + offset);
    dotdot_entry->inode_number = session_config->current_dir_inode; // Parent directory's inode
    dotdot_entry->name_length = 2;
    dotdot_entry->name[0] = '.';
    dotdot_entry->name[1] = '.';
    dotdot_entry->name[2] = '\0';
    dotdot_entry->record_length = sizeof(DirectoryEntry) + 3;
    offset += dotdot_entry->record_length;
    
    // Update directory inode file_size
    dir_inode->file_size = offset;
    
    // Add entry to parent directory's data block
    int result = add_directory_entry(session_config->current_dir_inode, dirname, new_inode_num);
    if (result != SUCCESS) {
        // Failed to add directory entry, clean up
        free_inode(new_inode_num);
        free_data_block(dir_data_block);
        return 0; // Return 0 on error
    }
    
    return new_inode_num;
}

int rmdir(const char *dirname)
{
    // TODO: Implement directory removal
    return SUCCESS;
}

void delete_directory_helper(const char *dirname)
{
    if (rmdir(dirname) == -1)
    {
        perror("rmdir");
        exit(EXIT_FAILURE);
    }
}

// Helper function: Get directory entry at a specific offset
DirectoryEntry* get_directory_entry_at_offset(uint8_t *dir_data, uint16_t offset)
{
    return (DirectoryEntry *)(dir_data + offset);
}

// Helper function: Get the offset of the next directory entry
uint16_t get_next_directory_entry_offset(uint8_t *dir_data, uint16_t current_offset, uint16_t dir_size)
{
    if (current_offset >= dir_size) {
        return dir_size; // Reached end
    }
    DirectoryEntry *entry = get_directory_entry_at_offset(dir_data, current_offset);
    uint16_t next_offset = current_offset + entry->record_length;
    return (next_offset > dir_size) ? dir_size : next_offset;
}

// Helper function: Find a directory entry by name in a directory
// Returns the inode number if found, 0 if not found
uint16_t find_directory_entry(uint16_t dir_inode, const char *name)
{
    if (name == NULL || strlen(name) == 0) {
        return 0;
    }
    
    // Get the directory's inode
    uint16_t inode_block = INODE_START + (dir_inode / 32);
    uint16_t inode_offset = (dir_inode % 32) * sizeof(Inode);
    Inode *dir_inode_ptr = (Inode *)(HARD_DISK[inode_block] + inode_offset);
    
    // Check if it's actually a directory
    if ((dir_inode_ptr->flags & 2) == 0) {
        return 0; // Not a directory
    }
    
    // Get the directory's first data block
    uint16_t dir_data_block = dir_inode_ptr->directBlocks[0];
    if (dir_data_block == 0) {
        return 0; // No data block
    }
    
    uint8_t *dir_data = HARD_DISK[dir_data_block];
    uint16_t dir_size = dir_inode_ptr->file_size;
    uint16_t offset = 0;
    
    // Iterate through directory entries
    while (offset < dir_size) {
        DirectoryEntry *entry = get_directory_entry_at_offset(dir_data, offset);
        
        // Skip . and .. entries
        if (entry->name_length > 0 && 
            !(entry->name_length == 1 && entry->name[0] == '.') &&
            !(entry->name_length == 2 && entry->name[0] == '.' && entry->name[1] == '.')) {
            
            // Compare names
            if (entry->name_length == strlen(name) && 
                strncmp(entry->name, name, entry->name_length) == 0) {
                return entry->inode_number;
            }
        }
        
        // Move to next entry
        offset = get_next_directory_entry_offset(dir_data, offset, dir_size);
    }
    
    return 0; // Not found
}

// Helper function: Add a directory entry to a directory's data block
// Returns SUCCESS on success, error code on failure
int add_directory_entry(uint16_t dir_inode, const char *name, uint16_t target_inode)
{
    if (name == NULL || strlen(name) == 0 || strlen(name) > MAX_FILENAME) {
        return ERROR_INVALID_INPUT;
    }
    
    // Check if entry already exists
    if (find_directory_entry(dir_inode, name) != 0) {
        return ERROR_INVALID_INPUT; // Entry already exists
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
        return ERROR_INVALID_INPUT; // No data block
    }
    
    uint8_t *dir_data = HARD_DISK[dir_data_block];
    uint16_t current_size = dir_inode_ptr->file_size;
    
    // Check if we have space (each block is 2048 bytes)
    uint16_t name_len = strlen(name);
    uint16_t entry_size = sizeof(DirectoryEntry) + name_len + 1; // +1 for null terminator
    
    if (current_size + entry_size > BLOCK_SIZE_BYTES) {
        // TODO: Handle case where we need to allocate another block
        return ERROR_INVALID_INPUT; // Not enough space in current block
    }
    
    // Create the new directory entry at the end
    DirectoryEntry *new_entry = (DirectoryEntry *)(dir_data + current_size);
    new_entry->inode_number = target_inode;
    new_entry->name_length = name_len;
    new_entry->record_length = entry_size;
    
    // Copy the name (including null terminator)
    memcpy(new_entry->name, name, name_len);
    new_entry->name[name_len] = '\0';
    
    // Update directory inode's file_size
    dir_inode_ptr->file_size = current_size + entry_size;
    dir_inode_ptr->mtime = time(NULL); // Update modification time
    
    // Write back the updated inode
    memcpy(HARD_DISK[inode_block] + inode_offset, dir_inode_ptr, sizeof(Inode));
    
    return SUCCESS;
}
