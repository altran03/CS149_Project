#include "headers/common.h"
#include "headers/utils.h"

HardDisk* disk = NULL;

// Initialize the disk
int init_disk(void) {
    if (disk != NULL) {
        return SUCCESS; // Already initialized
    }

    disk = (HardDisk*)malloc(sizeof(HardDisk));
    if (disk == NULL) {
        return ERROR_INVALID_INPUT;
    }

    // Initialize all blocks to zero
    memset(disk->blocks, 0, TOTAL_BLOCKS * BLOCK_SIZE);
    
    // Initialize free bit array (all blocks free initially, except system blocks)
    memset(disk->free_bit_array, 0xFF, BLOCK_SIZE); // Set all bits to 1 (free)
    
    // Mark system blocks as used (0-67)
    for (int i = 0; i <= DATA_START_BLOCK; i++) {
        int byte_index = i / 8;
        int bit_index = i % 8;
        disk->free_bit_array[byte_index] &= ~(1 << bit_index); // Set bit to 0 (used)
    }

    // Initialize FCB array
    memset(disk->fcb_array, 0, FCB_BLOCKS * sizeof(FCB));
    
    // Initialize directory node cache
    memset(disk->dir_node_cache, 0, FCB_BLOCKS * sizeof(DirNode*));
    
    // Initialize file descriptor table
    disk->fd_table = (FileDescriptorTable*)malloc(sizeof(FileDescriptorTable));
    if (disk->fd_table == NULL) {
        free(disk);
        disk = NULL;
        return ERROR_INVALID_INPUT;
    }
    memset(disk->fd_table->descriptors, 0, MAX_OPEN_FILES * sizeof(FileDescriptor));
    disk->fd_table->count = 0;

    // Allocate FCB for root directory
    uint16_t root_fcb = allocate_fcb();
    if (root_fcb == 0xFFFF) {
        // Error allocating FCB
        free(disk->fd_table);
        free(disk);
        disk = NULL;
        return ERROR_INVALID_INPUT;
    }
    
    FCB* root_fcb_ptr = get_fcb(root_fcb);
    root_fcb_ptr->file_type = FILE_TYPE_DIRECTORY;
    root_fcb_ptr->size = 0;
    root_fcb_ptr->owner_id = getuid();
    root_fcb_ptr->permissions = PERM_READ | PERM_WRITE | PERM_EXEC;
    uint32_t current_time = get_current_time();
    root_fcb_ptr->access_time = current_time;
    root_fcb_ptr->modify_time = current_time;
    root_fcb_ptr->change_time = current_time;
    
    // Create root directory node
    disk->root_directory = get_or_create_dir_node(root_fcb, "/", NULL);
    if (disk->root_directory == NULL) {
        free_fcb(root_fcb);
        free(disk->fd_table);
        free(disk);
        disk = NULL;
        return ERROR_INVALID_INPUT;
    }

    disk->initialized = 1;
    return SUCCESS;
}

// Cleanup disk
void cleanup_disk(void) {
    if (disk == NULL) {
        return;
    }

    // Free directory tree (recursive cleanup would be needed for full implementation)
    if (disk->root_directory != NULL) {
        if (disk->root_directory->table != NULL) {
            // Free all directory entries
            for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
                DirEntry* entry = disk->root_directory->table->entries[i];
                while (entry != NULL) {
                    DirEntry* next = entry->next;
                    free(entry);
                    entry = next;
                }
            }
            free(disk->root_directory->table);
        }
        free(disk->root_directory);
    }

    if (disk->fd_table != NULL) {
        free(disk->fd_table);
    }

    free(disk);
    disk = NULL;
}

// Get current time as 32-bit timestamp
uint32_t get_current_time(void) {
    return (uint32_t)time(NULL);
}

// Allocate a free block
int allocate_block(void) {
    if (disk == NULL) {
        return -1;
    }

    for (int i = DATA_START_BLOCK + 1; i < TOTAL_BLOCKS; i++) {
        if (is_block_free(i)) {
            int byte_index = i / 8;
            int bit_index = i % 8;
            disk->free_bit_array[byte_index] &= ~(1 << bit_index); // Mark as used
            return i;
        }
    }
    return -1; // No free blocks
}

// Free a block
void free_block(int block_num) {
    if (disk == NULL || block_num < 0 || block_num >= TOTAL_BLOCKS) {
        return;
    }

    if (block_num <= DATA_START_BLOCK) {
        return; // Cannot free system blocks
    }

    int byte_index = block_num / 8;
    int bit_index = block_num % 8;
    disk->free_bit_array[byte_index] |= (1 << bit_index); // Mark as free
}

// Check if a block is free
int is_block_free(int block_num) {
    if (disk == NULL || block_num < 0 || block_num >= TOTAL_BLOCKS) {
        return 0;
    }

    int byte_index = block_num / 8;
    int bit_index = block_num % 8;
    return (disk->free_bit_array[byte_index] >> bit_index) & 1;
}

