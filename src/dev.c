#include "headers/common.h"
#include "headers/utils.h"
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

// Global session configuration
SessionConfig *session_config;

// HARD DISK - actual storage array
// 16 bits is enough for number of blocks 2^16=65536>16384, each block is 2KiB
uint8_t HARD_DISK[BLOCK_NUM][BLOCK_SIZE_BYTES];

//PROTOTYPES
uint16_t fs_open(const char *pathname, uint16_t operation);
int fs_close(uint16_t file_descriptor);

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
    dot_entry->inode_number = INODE_START; // Root inode
    dot_entry->name_length = 1;
    dot_entry->name[0] = '.';
    dot_entry->name[1] = '\0';
    dot_entry->record_length = sizeof(DirectoryEntry) + 2; // 2 bytes for "." + null terminator
    offset += dot_entry->record_length;
    
    // Create .. entry (also points to root inode since root is its own parent)
    DirectoryEntry *dotdot_entry = (DirectoryEntry *)(root_data_block + offset);
    dotdot_entry->inode_number = INODE_START;
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
    // TODO: Get parent directory's inode number from current working directory
    // For now, assuming parent is root (INODE_START)
    DirectoryEntry *dotdot_entry = (DirectoryEntry *)(dir_data + offset);

    //get parent inode, first open current working dir and get file_descriptor
    uint16_t cwd_fd = fs_open(session_config->current_working_dir, 420); 
    uint16_t result[2];
    //get the disk location of the fd, store in result
    fd_number_to_disk_location(cwd_fd, result);
    //copy from hard disk to cwd_file_descriptor
    FileDescriptor *cwd_file_descriptor;
    memcpy(cwd_file_descriptor, HARD_DISK[result[0]]+result[1], sizeof(FileDescriptor));
    //set current working directory inode as parent inode
    dotdot_entry->inode_number = cwd_file_descriptor->inode_number;
    //close file descriptor after done using
    fs_close(cwd_fd);

    dotdot_entry->name_length = 2;
    dotdot_entry->name[0] = '.';
    dotdot_entry->name[1] = '.';
    dotdot_entry->name[2] = '\0';
    dotdot_entry->record_length = sizeof(DirectoryEntry) + 3;
    offset += dotdot_entry->record_length;
    
    // Update directory inode file_size
    dir_inode->file_size = offset;
    
    // TODO: Add entry to parent directory's data block
    // This requires reading parent directory, finding space, adding entry
    
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
    DirectoryEntry *file = malloc(sizeof(DirectoryEntry));
    if (file == NULL)
    {
        return ERROR_INVALID_INPUT; // Memory allocation failed
    }
    file->inode_number = free_inode;

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
    // TODO: Implement file deletion
    return SUCCESS;
}

// assuming /foo/bar is pathname and op is O_RDONLY
uint16_t fs_open(const char *const_pathname, uint16_t operation)
{
    char* pathname;
    strcpy(pathname, const_pathname);
    char* dirEntry;
    // root directory is / in pathname
    // start at root, then so read in inode, inode block 1 which is block INODE_START 2
    Inode rootInode;
    memcpy(&rootInode, HARD_DISK[INODE_START], sizeof(Inode));
    DirectoryEntry *curEntry = (DirectoryEntry *)HARD_DISK[ROOT_DIRECTORY];

    //split pathname by /
    dirEntry = strtok(pathname, "/"); 

    while(dirEntry != NULL) {
        //iterate through names.entries until find next entry that matches path
        uint8_t i = 0; 
        bool found = false;

        while (i < curEntry->entries_length){
            DirectoryEntry *nextEntry = curEntry->entries[i];
            //if matches change curEntry to nextEntry
            if (strcmp(nextEntry->name, dirEntry) == 0){
                curEntry = nextEntry;
                found = true;
                break; //found so stop looking
            }
            i++;
        }

        if (!found){
            printf("Pathname not found!, full_path: %s, missing: %s", pathname, dirEntry);
            return -1;
        }
        dirEntry = strtok(NULL, "/");
    }
 
    Inode in;
    // after finding bar, read in its inode
    uint16_t inode_place[2];
    inode_number_to_disk_location(curEntry->inode_number, inode_place);
    memcpy(&in, HARD_DISK[inode_place[0]] + inode_place[1], sizeof(Inode));

    FileDescriptor *fd;
    fd = (FileDescriptor *)malloc(sizeof(FileDescriptor *));
    fd->inode_number = curEntry->inode_number;
    //0 means not found/created, so if body is for created already
    uint16_t fd_num = is_fd_created(fd);
    uint16_t result[2];
    if (fd_num != 0) {
        //read from hard disk
        fd_number_to_disk_location(fd_num, result);
        memcpy(fd, HARD_DISK[result[0]] + result[1], sizeof(FileDescriptor));
        //increment reference count
        fd->referenceCount++;
        free(fd);
        return fd_num;
    }
    //check bar.permissions if operation is allowed, operation should not change with &
    else if (operation & in.permissions == operation) {  
        //create FileDescriptor and return
        fd->offset = 0; //start at 0 for read/write, can later change for append
        fd->referenceCount = 1;

        //allocate to hard disk
        fd_num = find_free_file_descriptor();
        fd_number_to_disk_location(fd_num, result);
        memcpy(HARD_DISK[result[0]] + result[1], fd, sizeof(FileDescriptor));
        free(fd);
        return fd_num;
    }
    // allocate file descriptor for process in File Descriptor Table, return to user file descriptor index

    return 0; // Return file descriptor or error
}

int fs_close(uint16_t file_descriptor)
{
    // deallocate file descriptor
    // TODO: Implement file closing
    uint16_t result[2];
    fd_number_to_disk_location(file_descriptor, result);
    memset(HARD_DISK[result[0]] + result[1], 0, sizeof(FileDescriptor));
       
    return SUCCESS;
}

int fs_read(uint16_t file_descriptor)
{
    // read first data block of file, consult inode
    // update last accessed time in inode
    // TODO: Implement file reading
    return SUCCESS;
}

int fs_write(uint16_t file_descriptor)
{
    // may allocate a block
    // each write generates 5 I/Os
    /*
    . Thus, each write to a
file logically generates five I/Os: one to read the data bitmap (which is
then updated to mark the newly-allocated block as used), one to write the
bitmap (to reflect its new state to disk), two more to read and then write
the inode (which is updated with the new block's location), and finally
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
    // TODO: Implement file writing
    return SUCCESS;
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

    create_root_directory();
    Inode rootInode;
    memcpy(&rootInode, HARD_DISK[INODE_START], sizeof(Inode));
    printf("Printing Root Inode:\n");
    printf("\tCreation Time: %s", ctime(&rootInode.ctime));
    printf("\tStarting Block: %d\n", rootInode.directBlocks[0]);
    printf("\tPermissions: %o\n", rootInode.permissions);
    printf("\tOwner/User ID: %d\n", rootInode.ownerID);
    printf("\tFile Size: %u bytes\n", rootInode.file_size);
    printf("Printing Root Directory Data Block:\n");
    DirectoryEntry *entry = (DirectoryEntry *)HARD_DISK[ROOT_DIRECTORY];
    printf("\tEntry 1 - Inode: %d, Name: %.*s\n", entry->inode_number, entry->name_length, entry->name);
    entry = (DirectoryEntry *)((uint8_t *)entry + entry->record_length);
    printf("\tEntry 2 - Inode: %d, Name: %.*s\n", entry->inode_number, entry->name_length, entry->name);
    
    uint16_t home_inode_num = create_directory("home");
    if (home_inode_num != 0) {
        printf("Created directory 'home' with inode: %d\n", home_inode_num);
    } else {
        printf("Failed to create directory 'home'\n");
    }
    Inode *home_inode;
    uint16_t home_inode_res[2];
    inode_number_to_disk_location(home_inode_num, home_inode_res);
    memcpy(home_inode, HARD_DISK[home_inode_res[0]]+home_inode_res[1], sizeof(home_inode));

    DirectoryEntry *homeDir = (DirectoryEntry *)HARD_DISK[home_inode->directBlocks[0]];
    printf("\tEntry 1 - Inode: %d, Name: %.*s\n", homeDir->inode_number, homeDir->name_length, homeDir->name);
    entry = (DirectoryEntry *)((uint8_t *)entry + homeDir->record_length);
    printf("\tEntry 2 - Inode: %d, Name: %.*s\n", homeDir->inode_number, homeDir->name_length, homeDir->name);
    uint16_t home_fd = ("/home", 420);
    uint16_t result[2];
    fd_number_to_disk_location(home_fd, result);
    FileDescriptor *home_file_descriptor;
    memcpy(home_file_descriptor, HARD_DISK[result[0]]+result[1], sizeof(FileDescriptor));
    printf("Printing Home File Descriptor, fd: %d", home_fd);
    printf("\tInode Number: %d", home_file_descriptor->inode_number);
    printf("\tFlags: %o", home_file_descriptor->flags);
    printf("\tReference Count/Concurrent Accesses: %ld", home_file_descriptor->offset);

    return 0;
}
