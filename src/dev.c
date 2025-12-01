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
    uint16_t uid; // current user id
    char current_working_dir[MAX_PATH_LENGTH];
    bool show_hidden_files;
    bool verbose_mode;
} SessionConfig;

SessionConfig *session_config;

// HARD DISK
#define BLOCK_NUM 16384       // each block is 2KiB (2^11), so total size = 2^11*16384(2^14) = 32MiB(2^25)
#define BLOCK_SIZE_BYTES 2048 // 2^11 bytes

// Reserved Block Numbers
#define SUPERBLOCK 0
#define FREE_BITMAP 1
#define INODE_START 2      // Also block containing Root inode (metadata)
#define INODE_END 513      // 512 Inode blocks for each block, each Inode block has 32 Inodes, in total 2^14 inodes, one for each block
#define ROOT_DIRECTORY 514 // First Block containing Root data
#define DATA_START 515     // start of data
#define DATA_END 16384     // last block

/*
16 bits is enough for number of blocks 2^16=65536>16384, each block is 2KiB, 16384, 2048
uint8_t, one byte is 8 bits
*/
uint8_t HARD_DISK[BLOCK_NUM][BLOCK_SIZE_BYTES];

// 64 bytes
typedef struct
{ // the Inode stores metadata about a file or directory

    uint16_t ownerID;
    uint16_t permissions;           // 0000000rwxrwxrwx, owner, group, other/world, 0 would be -/no and 1 would be yes
    uint32_t file_size;             // in bytes
    uint16_t directBlocks[6];       // this can be reduced for more metadata options
    uint16_t indirect;              // 0 means uninitialized
    uint16_t second_level_indirect; // 0 means uninitialized
    // we're okay that it isn't 2038 yet to store time in 32 bits
    time_t time;    // last accessed
    time_t ctime;   // creation time
    time_t mtime;   // last modified
    time_t dtime;   // file deletion time
    uint32_t flags; // 1 regular file, 2 directory, 4 indirect block, 8 second_level_indirect block, etc.

} Inode;
static_assert(sizeof(Inode) == 64, "Inode must be 64 bytes in size");

int init_inodes(const char *filename)
{
}

typedef struct
{
    uint16_t inode_number; // inode of entry
    uint8_t record_length; // total bytes for name + leftover space
    uint8_t str_length;    // total bytes/char for name
    char name[];           // flexible array for name string, at most 256 char considering str_length
} DirectoryEntry;

typedef struct
{
    uint16_t inode_number;    // inode of directory
    uint8_t length;           // number of entries
    DirectoryEntry **entries; // pointer to array of pointers, because DirectoryEntries not contiguous
    char *path;               // pointer for path
} Directory;

typedef struct
{
    uint16_t inode_number; // inode of entry
    uint8_t record_length; // total bytes for name + leftover space
    uint8_t str_length;    // total bytes/char for name
    char name[];           // flexible array for name string, at most 256 considering str_length
} DirectoryEntry;
typedef struct
{
    uint16_t inode_number; // inode of file
    char *path;            // pointer for path
    uint32_t file_size;    // current file size in bytes
} File;

int init_directory(const char *filename, Directory *dir)
{
    // find free inode
    // set dir.inode_number to free inode and change inode status to in use in bitmap
    // initialize DirectoryEntry for . (this directory) and .. (parent directory)
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
    memcpy(HARD_DISK[INODE_START], in, sizeof(Inode));
}

// does both directory and inode, for now pases back directory*, maybe change to void
Directory *create_root_directory()
{
    Directory *dir = malloc(sizeof(Directory));
    dir->inode_number = INODE_START;
    dir->length = 0;
    dir->path = malloc(sizeof(char) * 2); // '/' and '\0' null terminator
    dir->path = "/";
    dir->entries = NULL;
    init_root_inode();
    return dir;
}

int find_free_data_block()
{
    // TODO: Implement bitmap-based data block allocation
    // This should check the data bitmap (block FREE_BITMAP) to find a free data block
    // Return the block number of a free data block, or 0 if none available
    return 0;
}

int find_free_inode()
{
    // TODO: Implement bitmap-based inode allocation
    // This should check the inode bitmap (block FREE_BITMAP) to find a free inode
    // Return the inode number of a free inode, or 0 if none available
    return 0;
}

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

void create_directory(const char *dirname)
{
    Directory *dir = malloc(sizeof(Directory));
    dir->inode_number = find_free_inode(); // find free inode
    dir->length = 0;
    dir->path = malloc(strlen(session_config->current_working_dir) + strlen(dirname) + 2);                                 // 2 for / and \0 null terminator
    if (snprintf(dir->path, MAX_PATH_LENGTH - 1, "%s/%s", session_config->current_working_dir, dirname) > MAX_PATH_LENGTH) // size check
        printf("MAX_PATH_LENGTH exceeded\n");
    memcpy(session_config->current_working_dir, dir->path, strlen(dir->path) + 1); // update current working dir as well
    dir->entries = NULL;                                                           // No entries initially
}

int rmdir(const char *dirname)
{
}

void delete_directory_helper(const char *dirname)
{
    if (rmdir(dirname) == -1)
    {
        perror("rmdir");
        exit(EXIT_FAILURE);
    }
}

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

int create_file(const char *filename)
{
    // Validate input
    if (filename == NULL || strlen(filename) == 0)
    {
        return ERROR_INVALID_INPUT;
    }

    if (strlen(filename) > MAX_FILENAME)
    {
        return ERROR_INVALID_INPUT;
    }

    // Find a free inode for the new file
    uint16_t free_inode = find_free_inode();
    if (free_inode == 0)
    {
        // Assuming find_free_inode returns 0 when no free inode is found
        return ERROR_FILE_NOT_FOUND; // Or we could define a new error code like ERROR_NO_FREE_INODE
    }

    // Initialize the inode for the file
    init_file_inode(free_inode);

    // Create File struct (similar to Directory)
    File *file = malloc(sizeof(File));
    if (file == NULL)
    {
        return ERROR_INVALID_INPUT; // Memory allocation failed
    }
    file->inode_number = free_inode;
    file->file_size = 0;

    // Allocate and set the path
    size_t path_len = strlen(session_config->current_working_dir) + strlen(filename) + 2; // 2 for / and \0
    file->path = malloc(path_len);
    if (file->path == NULL)
    {
        free(file);
        return ERROR_INVALID_INPUT; // Memory allocation failed
    }

    if (snprintf(file->path, path_len, "%s/%s", session_config->current_working_dir, filename) >= (int)path_len)
    {
        printf("MAX_PATH_LENGTH exceeded\n");
        free(file->path);
        free(file);
        return ERROR_INVALID_INPUT;
    }

    // TODO: Add DirectoryEntry to current directory
    // 1. Reading the current directory's inode
    // 2. Reading the directory's data block(s)
    // 3. Creating a new DirectoryEntry with the filename and inode number
    // 4. Adding it to the directory entries
    // 5. Updating the directory's inode

    // For now, we've created the file's inode and File struct
    // The directory entry addition can be implemented later when directory operations are complete

    // Note: File struct should be managed by the caller
    // or stored in a file table, freeing it for now
    free(file->path);
    free(file);

    return SUCCESS;
}

int delete_file(const char *filename)
{
}

typedef enum
{
    O_RDONLY // read only
} Operation;

// assuming /foo/bar is pathname and op is O_RDONLY
int open(const char *pathname, Operation op)
{
    // root directory is / in pathname
    // start at root, then so read in inode, inode block 1 which is block INODE_START 2
    // inside the inode should have pointers to data blocks, first is ROOT_DIRECTORY 514, but can have more
    // then starts to traverse entries to find entry.name==foo, from there get the entry inode and do recursion for bar
    // after finding bar, read in its inode, check bar.permissions if operation is allowed, probably bit operation like r & op == 1
    // allocate file descriptor for process in File Descriptor Table, return to user file descriptor pointer
}

int close(int file_descriptor)
{
    // deallocate file descriptor
}

int read(int file_descriptor)
{
    // read first data block of file, consult inode
    // update last accessed time in inode
}

int write(int file_descriptor)
{
    // may allocate a block
    // each write generates 5 I/Os
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
int main()
{
    session_config = (SessionConfig *)malloc(sizeof(SessionConfig));
    session_config->uid = 0;
    strcpy(session_config->current_working_dir, "/"); // Initialize current working directory

    printf("Number of blocks is %d\n", BLOCK_NUM);
    printf("Size of Block is %d bytes\n", BLOCK_SIZE_BYTES);
    printf("Size of Inode is %ld bytes\n", sizeof(Inode)); // 64
    printf("Number of Inodes is %ld in %d inode blocks\n", (INODE_END - INODE_START + 1) * (BLOCK_SIZE_BYTES / sizeof(Inode)),
           (INODE_END - INODE_START + 1));                              // 16384
    printf("Number of data blocks is %d\n", DATA_END - DATA_START + 1); // 15870

    // 48 is the ASCII charcter 0 to see for printing
    memset(HARD_DISK, 48, sizeof(HARD_DISK));
    // prints first block
    // print_hard_disk_ascii(0, 1); //printed 2048 0s
    // reset_HARD_DISK fills hard disk with 0s
    reset_hard_disk();

    Directory *rootDirectory;
    rootDirectory = create_root_directory();
    Inode rootInode;
    memcpy(&rootInode, HARD_DISK[INODE_START], sizeof(Inode));
    printf("Printing Root Inode:\n");
    printf("\tCreation Time: %s", ctime(&rootInode.ctime));
    printf("\tStarting Block: %d\n", rootInode.directBlocks[0]);
    printf("\tPermissions: %o\n", rootInode.permissions);
    printf("\tOwner/User ID: %d\n", rootInode.ownerID);
    printf("Printing Root Directory:\n");
    // printf("\tEntries: %s", rootDirectory->entries[0]); save this for later
    printf("\tInode Number Block: %d\n", rootDirectory->inode_number);
    printf("\tLength: %d\n", rootDirectory->length);
    printf("\tPath: %s\n", rootDirectory->path);
    create_directory("home");
    printf("Current Working Directory: %s\n", session_config->current_working_dir);
    return 0;
}
