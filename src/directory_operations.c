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

extern SessionConfig *session_config;
extern uint8_t HARD_DISK[BLOCK_NUM][BLOCK_SIZE_BYTES];

void init_root_inode()
{
    Inode *in = malloc(sizeof(Inode));
    in->ownerID = session_config->uid;
    in->permissions = 420;
    in->file_size = 0;
    in->directBlocks[0] = ROOT_DIRECTORY;
    in->indirect = 0;
    in->second_level_indirect = 0;
    in->time = time(NULL);
    in->ctime = time(NULL);
    in->mtime = time(NULL);
    in->dtime = 0;
    in->flags = 2;
    memcpy(HARD_DISK[INODE_START], in, sizeof(Inode));
}

void create_root_directory(){
    init_root_inode();
    
    uint8_t *root_data_block = HARD_DISK[ROOT_DIRECTORY];
    uint16_t offset = 0;
    
    DirectoryEntry *dot_entry = (DirectoryEntry *)(root_data_block + offset);
    dot_entry->inode_number = 0;
    dot_entry->name_length = 1;
    dot_entry->name[0] = '.';
    dot_entry->name[1] = '\0';
    dot_entry->record_length = sizeof(DirectoryEntry) + 2;
    offset += dot_entry->record_length;
    
    DirectoryEntry *dotdot_entry = (DirectoryEntry *)(root_data_block + offset);
    dotdot_entry->inode_number = 0;
    dotdot_entry->name_length = 2;
    dotdot_entry->name[0] = '.';
    dotdot_entry->name[1] = '.';
    dotdot_entry->name[2] = '\0';
    dotdot_entry->record_length = sizeof(DirectoryEntry) + 3;
    
    Inode *root_inode = (Inode *)HARD_DISK[INODE_START];
    root_inode->file_size = dot_entry->record_length + dotdot_entry->record_length;
}

void init_inode(uint16_t inode_number)
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
    in->flags = 2;

    uint16_t inode_block = INODE_START + (inode_number / 32);
    uint16_t inode_offset = (inode_number % 32) * sizeof(Inode);

    memcpy(HARD_DISK[inode_block] + inode_offset, in, sizeof(Inode));
    free(in);
}

uint16_t create_directory(const char *dirname)
{
    if (dirname == NULL || strlen(dirname) == 0 || strlen(dirname) > MAX_FILENAME) {
        printf("INVALID dirname: %s\n", dirname);
        return 0;
    }
    
    uint16_t new_inode_num = find_free_inode();
    if (new_inode_num == 0) {
        printf("No free Inodes\n");
        return 0;
    }
    
    init_inode(new_inode_num);
    
    Inode *dir_inode = (Inode *)(HARD_DISK[INODE_START + (new_inode_num / 32)] + (new_inode_num % 32) * sizeof(Inode));
    uint16_t dir_data_block = dir_inode->directBlocks[0];
    uint8_t *dir_data = HARD_DISK[dir_data_block];
    uint16_t offset = 0;
    
    DirectoryEntry *dot_entry = (DirectoryEntry *)(dir_data + offset);
    dot_entry->inode_number = new_inode_num;
    dot_entry->name_length = 1;
    dot_entry->name[0] = '.';
    dot_entry->name[1] = '\0';
    dot_entry->record_length = sizeof(DirectoryEntry) + 2;
    offset += dot_entry->record_length;
    
    DirectoryEntry *dotdot_entry = (DirectoryEntry *)(dir_data + offset);
    dotdot_entry->inode_number = session_config->current_dir_inode;
    dotdot_entry->name_length = 2;
    dotdot_entry->name[0] = '.';
    dotdot_entry->name[1] = '.';
    dotdot_entry->name[2] = '\0';
    dotdot_entry->record_length = sizeof(DirectoryEntry) + 3;
    offset += dotdot_entry->record_length;
    
    dir_inode->file_size = offset;
    
    int result = add_directory_entry(session_config->current_dir_inode, dirname, new_inode_num);
    if (result != SUCCESS) {
        free_inode(new_inode_num);
        free_data_block(dir_data_block);
        return 0;
    }
    
    return new_inode_num;
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
        return dir_size;
    }
    DirectoryEntry *entry = get_directory_entry_at_offset(dir_data, current_offset);
    uint16_t next_offset = current_offset + entry->record_length;
    return (next_offset > dir_size) ? dir_size : next_offset;
}

// Helper function: Find a directory entry by name in a directory
uint16_t find_directory_entry(uint16_t dir_inode, const char *name)
{
    if (name == NULL || strlen(name) == 0) {
        return 0;
    }
    
    uint16_t inode_block = INODE_START + (dir_inode / 32);
    uint16_t inode_offset = (dir_inode % 32) * sizeof(Inode);
    Inode *dir_inode_ptr = (Inode *)(HARD_DISK[inode_block] + inode_offset);
    
    if ((dir_inode_ptr->flags & 2) == 0) {
        return 0;
    }
    
    uint16_t dir_data_block = dir_inode_ptr->directBlocks[0];
    if (dir_data_block == 0) {
        return 0;
    }
    
    uint8_t *dir_data = HARD_DISK[dir_data_block];
    uint16_t dir_size = dir_inode_ptr->file_size;
    uint16_t offset = 0;
    
    while (offset < dir_size) {
        DirectoryEntry *entry = get_directory_entry_at_offset(dir_data, offset);
        
        if (entry->name_length > 0 && 
            !(entry->name_length == 1 && entry->name[0] == '.') &&
            !(entry->name_length == 2 && entry->name[0] == '.' && entry->name[1] == '.')) {
            
            if (entry->name_length == strlen(name) && 
                strncmp(entry->name, name, entry->name_length) == 0) {
                return entry->inode_number;
            }
        }
        
        offset = get_next_directory_entry_offset(dir_data, offset, dir_size);
    }
    
    return 0;
}

// Helper function: Add a directory entry to a directory's data block
int add_directory_entry(uint16_t dir_inode, const char *name, uint16_t target_inode)
{
    if (name == NULL || strlen(name) == 0 || strlen(name) > MAX_FILENAME) {
        return ERROR_INVALID_INPUT;
    }
    
    if (find_directory_entry(dir_inode, name) != 0) {
        return ERROR_INVALID_INPUT;
    }
    
    uint16_t inode_block = INODE_START + (dir_inode / 32);
    uint16_t inode_offset = (dir_inode % 32) * sizeof(Inode);
    Inode *dir_inode_ptr = (Inode *)(HARD_DISK[inode_block] + inode_offset);
    
    if ((dir_inode_ptr->flags & 2) == 0) {
        return ERROR_INVALID_INPUT;
    }
    
    uint16_t dir_data_block = dir_inode_ptr->directBlocks[0];
    if (dir_data_block == 0) {
        return ERROR_INVALID_INPUT;
    }
    
    uint8_t *dir_data = HARD_DISK[dir_data_block];
    uint16_t current_size = dir_inode_ptr->file_size;
    
    uint16_t name_len = strlen(name);
    uint16_t entry_size = sizeof(DirectoryEntry) + name_len + 1;
    
    if (current_size + entry_size > BLOCK_SIZE_BYTES) {
        return ERROR_INVALID_INPUT;
    }
    
    DirectoryEntry *new_entry = (DirectoryEntry *)(dir_data + current_size);
    new_entry->inode_number = target_inode;
    new_entry->name_length = name_len;
    new_entry->record_length = entry_size;
    
    memcpy(new_entry->name, name, name_len);
    new_entry->name[name_len] = '\0';
    
    dir_inode_ptr->file_size = current_size + entry_size;
    dir_inode_ptr->mtime = time(NULL);
    
    memcpy(HARD_DISK[inode_block] + inode_offset, dir_inode_ptr, sizeof(Inode));
    
    return SUCCESS;
}
