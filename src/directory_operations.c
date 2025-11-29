#include "headers/directory_operations.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void create_directory(const char *dirname)
{
    Directory *dir = malloc(sizeof(Directory));
    dir->inode_number = 0; // Placeholder inode number
    dir->length = 0;
    dir->path = current_working_dir->path + "/" + strdup(dirname); // current_working_dir from SessionConfig
    dir->entries = NULL;                                           // No entries initially
}
void delete_directory_helper(const char *dirname)
{
    if (rmdir(dirname) == -1)
    {
        perror("rmdir");
        exit(EXIT_FAILURE);
    }
}
