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
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define MAX_PATH_LENGTH 1024
#define MAX_FILENAME 256
#define BUFFER_SIZE 4096

// Error codes
#define SUCCESS 0
#define ERROR_FILE_NOT_FOUND -1
#define ERROR_PERMISSION_DENIED -2
#define ERROR_INVALID_INPUT -3

typedef struct
{
    uint16_t uid; // current user id
    char current_working_dir[MAX_PATH_LENGTH];
    bool show_hidden_files;
    bool verbose_mode;
} SessionConfig;

// HARD DISK
#define BLOCK_NUM 16384
#define BLOCK_SIZE_BYTES 2048

// Reserved Block Numbers
#define SUPERBLOCK 0
#define FREE_INODE_BITMAP 1
#define FREE_KERNEL_BITMAP 2
#define FREE_DATA_BITMAP 3
#define INODE_START 4 // 512 Inode blocks for each block, each Inode block has 32 Inodes, in total 2^14 inodes, one for each block
#define INODE_END 515
#define ROOT_DIRECTORY 516
#define KERNEL_MEMORY_START 517 //stores FileDescriptors
#define KERNEL_MEMORY_END 533 //there are at most 4096 FileDescriptors so 12 < 16 bits
#define DATA_START 534 // start of data
#define DATA_END 16384 // last block

// 64 bytes
typedef struct
{ // The Inode stores metadata about a file or directory

    uint16_t ownerID;
    uint16_t permissions;           // 0000000rwxrwxrwx, owner, group, other/world
    uint32_t file_size;             // in bytes
    uint16_t directBlocks[6];       // this can be reduced for more metadata options
    uint16_t indirect;              // 0 means uninitialized
    uint16_t second_level_indirect; // 0 means uninitialized
    time_t time;    // last accessed
    time_t ctime;   // creation time
    time_t mtime;   // last modified
    time_t dtime;   // file deletion time
    uint32_t flags; // 1 regular file, 2 directory, 4 indirect block, 8 second_level_indirect block, etc.

} Inode;
static_assert(sizeof(Inode) == 64, "Inode must be 64 bytes in size");
// DirectoryEntry structure - represents a single entry in a directory
// On disk: directories are just sequences of DirectoryEntry structures
// In memory: directories are dynamic arrays/lists of DirectoryEntry - no Directory wrapper needed
// Note: Flexible array member - actual size varies based on name_length

struct DirectoryEntry_Struct; 

typedef struct DirectoryEntry_Struct {
    uint16_t inode_number; //inode of entry
    uint8_t record_length; //total bytes for name + leftover space
    uint8_t str_length; //total bytes/char for name
    uint8_t entries_length; //number of entries, once again for file is 0
    struct DirectoryEntry_Struct **entries; //for file is 0
    char *path; //pointer for path
    char name[]; //flexible array for name string, at most 256 char considering str_length
}DirectoryEntry;

//8 bytes
typedef struct {
    uint16_t inode_number;
    uint16_t offset; //in bytes for block, max should be 2048
    uint16_t flags; //file operation that will be compared with inode permissions
    uint16_t referenceCount; //number of concurrent references, 128 max can be increased if necessary
} FileDescriptor;
static_assert(sizeof(FileDescriptor) == 8, "FileDescriptor must be 64 bytes in size");

#endif