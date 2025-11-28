#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
// #include <dirent.h>
#include <time.h>
// #include <pwd.h>
// #include <grp.h>
#include <errno.h>
#include <stdint.h>
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

//HARD DISK
#define BLOCK_NUM 16384
#define BLOCK_SIZE_BYTES 2048

//Reserved Block Numbers
#define SUPERBLOCK 0
#define FREE_BITMAP 1
#define INODE_START 2 //512 Inode blocks for each block, each Inode block has 32 Inodes, in total 2^14 inodes, one for each block
#define INODE_END 513
#define ROOT_DIRECTORY 514
#define DATA_START 515 //start of data
#define DATA_END 16384 //last block

//64 bytes
typedef struct{

    uint16_t ownerID;
    uint16_t permissions;
    uint32_t file_size; //in bytes
    uint16_t directBlocks[16]; //this can be reduced for more metadata options
    uint16_t indirect;
    uint16_t second_level_indirect;
    uint32_t time; //last accessed
    uint32_t ctime; //creation time
    uint32_t mtime; //last modified
    uint32_t dtime; //file deletion time
    uint32_t flags; //1 regular file, 2 directory, 4 indirect block, 8 second_level_indirect block, etc.


} Inode;
static_assert(sizeof(Inode) == 64, "Inode must be 64 bytes in size");

typedef struct{
    uint16_t inode_number; //inode of directory
    uint8_t length; //number of entries
    DirectoryEntry **entries; //pointer to array of pointers, because DirectoryEntries not contiguous
}Directory;

typedef struct{
    uint16_t inode_number; //inode of entry
    uint8_t record_length; //total bytes for name + leftover space
    uint8_t str_length; //total bytes/char for name
    char name[]; //flexible array for name string, at most 256 considering str_length
}DirectoryEntry;

int init_directory(const char *filename, Directory *dir){
    //find free inode
    //set inode_number to free inode and change inode status to in use in bitmap
    //initialize DirectoryEntry for . (this directory) and .. (parent directory)
}

/*
16 bits is enough for number of blocks 2^16=65536>16384, each block is 2KiB, 16384, 2048
uint8_t, one byte is 8 bits
*/
uint8_t hard_disk[BLOCK_NUM][BLOCK_SIZE_BYTES]; 
int main(){
	
    printf("Number of blocks is %d\n", BLOCK_NUM);
    printf("Size of Block is %d bytes\n", BLOCK_SIZE_BYTES);
	printf("Size of Inode is %ld bytes\n", sizeof(Inode)); //64
    printf("Number of Inodes is %ld in %d inode blocks\n", (INODE_END-INODE_START+1)*(BLOCK_SIZE_BYTES/sizeof(Inode)), (INODE_END-INODE_START+1)); //16384
    printf("Number of data blocks is %d\n", DATA_END-DATA_START+1); //15870
    return 0;
	
}
