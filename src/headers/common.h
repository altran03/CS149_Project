#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <stdint.h>

// Constants
#define MAX_PATH_LENGTH 1024
#define MAX_FILENAME 256
#define BUFFER_SIZE 4096
#define BLOCK_SIZE 2048
#define TOTAL_BLOCKS 16384
#define TOTAL_DISK_SIZE (TOTAL_BLOCKS * BLOCK_SIZE) // 32 MiB
#define FCB_SIZE 64
#define FCB_BLOCKS 64
#define MAX_DIR_ENTRIES 128
#define MAX_OPEN_FILES 64
#define MAX_DIRECT_BLOCKS 18

// Block layout
#define SUPERBLOCK_NUM 0
#define FREE_BIT_ARRAY_BLOCK 1
#define FCB_START_BLOCK 2
#define FCB_END_BLOCK 65
#define ROOT_DIR_BLOCK 66
#define DATA_START_BLOCK 67

// Error codes
#define SUCCESS 0
#define ERROR_FILE_NOT_FOUND -1
#define ERROR_PERMISSION_DENIED -2
#define ERROR_INVALID_INPUT -3
#define ERROR_FILE_EXISTS -4
#define ERROR_DIR_NOT_FOUND -5
#define ERROR_DISK_FULL -6
#define ERROR_INVALID_MODE -7
#define ERROR_FILE_NOT_OPEN -8

// File modes
#define MODE_READ 0
#define MODE_WRITE 1
#define MODE_APPEND 2

// File types
#define FILE_TYPE_REGULAR 0
#define FILE_TYPE_DIRECTORY 1

// Permission bits
#define PERM_READ 4
#define PERM_WRITE 2
#define PERM_EXEC 1

// File Control Block (64 bytes)
typedef struct {
    uint16_t owner_id;              // 16 bit owner/user id
    uint16_t permissions;            // 16 bit access permissions (rwx)
    uint16_t direct_blocks[18];     // 18 direct location (memory block) 16 bit unsigned integers
    uint16_t indirect_block_1;      // 1st level indirect memory block number
    uint16_t indirect_block_2;      // 2nd level indirect memory block number
    uint16_t file_type;             // 16 bit file type
    uint32_t size;                  // 32 bit size in bytes
    uint32_t access_time;           // 32 bit access (r, read) time
    uint32_t modify_time;           // 32 bit modification (m, write) time
    uint32_t change_time;           // 32 bit metadata change (c, permissions etc.) time
} FCB;

// Directory Entry
typedef struct DirEntry {
    char name[MAX_FILENAME];
    uint16_t fcb_index;             // Index into FCB array (0-63)
    uint8_t is_directory;           // 1 if directory, 0 if file
    struct DirEntry* next;          // For hash table chaining
} DirEntry;

// Directory Table (Hash table for directory contents)
typedef struct {
    DirEntry* entries[MAX_DIR_ENTRIES];
    int count;
} DirectoryTable;

// Directory Node
typedef struct DirNode {
    char name[MAX_FILENAME];
    struct DirNode* parent;
    DirectoryTable* table;
    uint16_t fcb_index;             // FCB index for this directory
} DirNode;

// File Descriptor Table Entry
typedef struct {
    FCB* fcb;                       // Pointer to FCB
    int mode;                       // Mode: read, write, append
    int ref_count;                  // Reference count (concurrent users)
    uint32_t offset;                // Offset of byte to be read/written
    int is_valid;                   // 1 if entry is in use
} FileDescriptor;

// File Descriptor Table
typedef struct {
    FileDescriptor descriptors[MAX_OPEN_FILES];
    int count;
} FileDescriptorTable;

// Hard Disk Structure
typedef struct {
    uint8_t blocks[TOTAL_BLOCKS][BLOCK_SIZE];
    uint8_t free_bit_array[BLOCK_SIZE];  // Bit array for free block management
    FCB fcb_array[FCB_BLOCKS];            // Array of FCBs
    DirNode* root_directory;              // Root directory
    DirNode* dir_node_cache[FCB_BLOCKS];  // Cache of directory nodes by FCB index
    FileDescriptorTable* fd_table;        // File descriptor table
    int initialized;                      // 1 if disk is initialized
} HardDisk;

// Global disk instance
extern HardDisk* disk;

// Function declarations
int init_disk(void);
void cleanup_disk(void);
uint32_t get_current_time(void);
int allocate_block(void);
void free_block(int block_num);
int is_block_free(int block_num);

#endif