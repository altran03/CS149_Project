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
    char current_working_dir[MAX_PATH_LENGTH];
    int show_hidden_files;
    int verbose_mode;
} SessionConfig;

// HARD DISK
#define BLOCK_NUM 16384
#define BLOCK_SIZE_BYTES 2048

// Reserved Block Numbers
#define SUPERBLOCK 0
#define FREE_BITMAP 1
#define INODE_START 2 // 512 Inode blocks for each block, each Inode block has 32 Inodes, in total 2^14 inodes, one for each block
#define INODE_END 513
#define ROOT_DIRECTORY 514
#define DATA_START 515 // start of data
#define DATA_END 16384 // last block

// 64 bytes
typedef struct
{ // The Inode stores metadata about a file or directory

    uint16_t ownerID;
    uint16_t permissions;
    uint32_t file_size;        // in bytes
    uint16_t directBlocks[16]; // this can be reduced for more metadata options
    uint16_t indirect;
    uint16_t second_level_indirect;
    uint32_t time;  // last accessed
    uint32_t ctime; // creation time
    uint32_t mtime; // last modified
    uint32_t dtime; // file deletion time
    uint32_t flags; // 1 regular file, 2 directory, 4 indirect block, 8 second_level_indirect block, etc.

} Inode;
static_assert(sizeof(Inode) == 64, "Inode must be 64 bytes in size");
typedef struct
{
    uint16_t inode_number;    // inode of directory
    uint8_t length;           // number of entries
    DirectoryEntry **entries; // pointer to array of pointers, because DirectoryEntries not contiguous
} Directory;

typedef struct
{
    char filename[MAX_FILENAME];
    uint16_t record_length; // length of this directory entry record
    uint8_t name_length;    // length of the filename
    uint16_t inode_index;   // index of the inode in the inode table
} DirectoryEntry;
static_assert(sizeof(DirectoryEntry) == 263, "DirectoryEntry must be 263 bytes in size");

#endif