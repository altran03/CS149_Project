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
#define INODE_START 2 //Also block containing Root inode (metadata)
#define INODE_END 513 //512 Inode blocks for each block, each Inode block has 32 Inodes, in total 2^14 inodes, one for each block
#define ROOT_DIRECTORY 514  //First Block containing Root data
#define DATA_START 515 //start of data
#define DATA_END 16384 //last block

//64 bytes
typedef struct{

    uint16_t ownerID;
    uint16_t permissions; //0000000rwxrwxrwx, owner, group, other/world, 0 would be -/no and 1 would be yes
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

int init_inode(const char *filename){

}

typedef struct{
    uint16_t inode_number; //inode of entry
    uint8_t record_length; //total bytes for name + leftover space
    uint8_t str_length; //total bytes/char for name
    char name[]; //flexible array for name string, at most 256 char considering str_length
}DirectoryEntry;

typedef struct{
    uint16_t inode_number; //inode of directory
    uint8_t length; //number of entries
    DirectoryEntry **entries; //pointer to array of pointers, because DirectoryEntries not contiguous
}Directory;

int init_directory(const char *filename, Directory *dir){
    //find free inode
    //set dir.inode_number to free inode and change inode status to in use in bitmap
    //initialize DirectoryEntry for . (this directory) and .. (parent directory)
}
int delete_file(const char *filename){

}

typedef enum {
    O_RDONLY //read only
}Operation;

//assuming /foo/bar is pathname and op is O_RDONLY
int open(const char *pathname, Operation op){
    //root directory is / in pathname
    //start at root, then so read in inode, inode block 1 which is block INODE_START 2
    //inside the inode should have pointers to data blocks, first is ROOT_DIRECTORY 514, but can have more
    //then starts to traverse entries to find entry.name==foo, from there get the entry inode and do recursion for bar
    //after finding bar, read in its inode, check bar.permissions if operation is allowed, probably bit operation like r & op == 1
    //allocate file descriptor for process in File Descriptor Table, return to user file descriptor pointer
}

int close(int file_descriptor){
    //deallocate file descriptor
}

int read(int file_descriptor){
    //read first data block of file, consult inode
    //update last accessed time in inode
}

int write(int file_descriptor){
    //may allocate a block
    //each write generates 5 I/Os
    /*
    . Thus, each write to a
file logically generates five I/Os: one to read the data bitmap (which is
then updated to mark the newly-allocated block as used), one to write the
bitmap (to reflect its new state to disk), two more to read and then write
the inode (which is updated with the new block’s location), and finally
one to write the actual block itself.
The amount of write traffic is even worse when one considers a simple and common operation such as file creation. To create a file, the file
system must not only allocate an inode, but also allocate space within
the directory containing the new file. The total amount of I/O traffic to
do so is quite high: one read to the inode bitmap (to find a free inode),
one write to the inode bitmap (to mark it allocated), one write to the new
inode itself (to initialize it), one to the data of the directory (to link the
high-level name of the file to its inode number), and one read and write
to the directory inode to update it. If the directory needs to grow to accommodate the new entry, additional I/Os (i.e., to the data bitmap, and
the new directory block) will be needed too. All that just to create a file!

You can also see that each allocating write
costs 5 I/Os: a pair to read and update the inode, another pair to read
and update the data bitmap, and then finally the write of the data itself.
How can a file system accomplish any of this with reasonable efficiency?

    */
}

/*
16 bits is enough for number of blocks 2^16=65536>16384, each block is 2KiB, 16384, 2048
uint8_t, one byte is 8 bits
*/
uint8_t HARD_DISK[BLOCK_NUM][BLOCK_SIZE_BYTES]; 
//creating the free bit map/array

//reset_HARD_DISK fills hard disk with 0s
void reset_hard_disk(){
    memset(HARD_DISK, 0, sizeof(HARD_DISK));
}

//print contents of hard disk in ascii, from starting block (inclusive) to end_block (exclusive/non-inclusive)
int print_hard_disk_ascii(int start_block, int end_block){
    if (start_block < 0 || end_block < 0 || start_block > BLOCK_NUM-1 || end_block > BLOCK_NUM){
        printf("Bounds of blocks should be between 0 and BLOCK_NUM, start: %d, end: %d\n", start_block, end_block);
        return 1;
    }
    for(int i = start_block; i < end_block; i++){
        for(int j = 0; j < BLOCK_SIZE_BYTES; j++){
            //check for printable character
            if (HARD_DISK[i][j] >= 32 && HARD_DISK[i][j] <= 126)
                printf("%c", HARD_DISK[i][j]);
            else
                printf(".");
        }
        printf("\n");
    }
    return 0;
}
int main(){
	
    printf("Number of blocks is %d\n", BLOCK_NUM);
    printf("Size of Block is %d bytes\n", BLOCK_SIZE_BYTES);
	printf("Size of Inode is %ld bytes\n", sizeof(Inode)); //64
    printf("Number of Inodes is %ld in %d inode blocks\n", (INODE_END-INODE_START+1)*(BLOCK_SIZE_BYTES/sizeof(Inode)),
     (INODE_END-INODE_START+1)); //16384
    printf("Number of data blocks is %d\n", DATA_END-DATA_START+1); //15870

    
    //48 is the ASCII charcter 0 to see for printing 
    memset(HARD_DISK, 48, sizeof(HARD_DISK));
    //prints first block
    print_hard_disk_ascii(0, 1); //printed 2048 0s
    //reset_HARD_DISK fills hard disk with 0s
    reset_hard_disk();
    return 0;
	
}
