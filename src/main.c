#include "headers/common.h"
#include "headers/file_operations.h"
#include "headers/directory_operations.h"
#include "headers/permissions.h"
#include "headers/utils.h"
#include <stdio.h>
#include <string.h>

void print_menu(void) {
    printf("\n=== File Management System ===\n");
    printf("1. createFile <name>\n");
    printf("2. openFile <name> <mode> (mode: 0=read, 1=write, 2=append)\n");
    printf("3. closeFile <fd>\n");
    printf("4. searchFile <name>\n");
    printf("5. mkDir <name>\n");
    printf("6. cd <path>\n");
    printf("7. chmod <name> <permissions> (e.g., rwx or 7)\n");
    printf("8. renameFile <oldName> <newName>\n");
    printf("9. moveFile <src> <destDir>\n");
    printf("10. list (list current directory)\n");
    printf("11. writeFile <fd> <data>\n");
    printf("12. readFile <fd> <size>\n");
    printf("13. deleteFile <name>\n");
    printf("14. pwd (print working directory)\n");
    printf("15. quit\n");
    printf("Enter command: ");
}

int main() {
    printf("Initializing File Management System...\n");
    
    if (init_disk() != SUCCESS) {
        fprintf(stderr, "Failed to initialize disk\n");
        return 1;
    }
    
    printf("File Management System initialized successfully!\n");
    printf("Disk size: %d MiB (%d blocks of %d bytes each)\n",
           TOTAL_DISK_SIZE / (1024 * 1024), TOTAL_BLOCKS, BLOCK_SIZE);
    
    char command[256];
    char arg1[256], arg2[256], arg3[256];
    
    while (1) {
        print_menu();
        
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        
        // Remove newline
        command[strcspn(command, "\n")] = 0;
        
        if (strlen(command) == 0) {
            continue;
        }
        
        // Parse command
        int num_args = sscanf(command, "%s %s %s %s", arg1, arg2, arg3, (char[256]){0});
        
        if (strcmp(arg1, "quit") == 0 || strcmp(arg1, "q") == 0) {
            break;
        } else if (strcmp(arg1, "createFile") == 0) {
            if (num_args < 2) {
                printf("Usage: createFile <name>\n");
                continue;
            }
            int result = createFile(arg2);
            if (result == SUCCESS) {
                printf("File '%s' created successfully\n", arg2);
            } else {
                printf("Error creating file: %d\n", result);
            }
        } else if (strcmp(arg1, "openFile") == 0) {
            if (num_args < 3) {
                printf("Usage: openFile <name> <mode>\n");
                continue;
            }
            int mode = atoi(arg3);
            int fd = openFile(arg2, mode);
            if (fd >= 0) {
                printf("File opened with file descriptor: %d\n", fd);
            } else {
                printf("Error opening file: %d\n", fd);
            }
        } else if (strcmp(arg1, "closeFile") == 0) {
            if (num_args < 2) {
                printf("Usage: closeFile <fd>\n");
                continue;
            }
            int fd = atoi(arg2);
            int result = closeFile(fd);
            if (result == SUCCESS) {
                printf("File descriptor %d closed\n", fd);
            } else {
                printf("Error closing file: %d\n", result);
            }
        } else if (strcmp(arg1, "searchFile") == 0) {
            if (num_args < 2) {
                printf("Usage: searchFile <name>\n");
                continue;
            }
            int result = searchFile(arg2);
            if (result != SUCCESS) {
                printf("Error searching file: %d\n", result);
            }
        } else if (strcmp(arg1, "mkDir") == 0) {
            if (num_args < 2) {
                printf("Usage: mkDir <name>\n");
                continue;
            }
            int result = mkDir(arg2);
            if (result == SUCCESS) {
                printf("Directory '%s' created successfully\n", arg2);
            } else {
                printf("Error creating directory: %d\n", result);
            }
        } else if (strcmp(arg1, "cd") == 0) {
            if (num_args < 2) {
                printf("Usage: cd <path>\n");
                continue;
            }
            int result = cd(arg2);
            if (result == SUCCESS) {
                printf("Changed to directory: %s\n", get_absolute_path(get_current_directory()));
            } else {
                printf("Error changing directory: %d\n", result);
            }
        } else if (strcmp(arg1, "chmod") == 0) {
            if (num_args < 3) {
                printf("Usage: chmod <name> <permissions>\n");
                continue;
            }
            uint16_t perms = parse_permissions(arg3);
            int result = chmod(arg2, perms);
            if (result == SUCCESS) {
                printf("Permissions changed for '%s'\n", arg2);
            } else {
                printf("Error changing permissions: %d\n", result);
            }
        } else if (strcmp(arg1, "renameFile") == 0) {
            if (num_args < 3) {
                printf("Usage: renameFile <oldName> <newName>\n");
                continue;
            }
            int result = renameFile(arg2, arg3);
            if (result == SUCCESS) {
                printf("File renamed from '%s' to '%s'\n", arg2, arg3);
            } else {
                printf("Error renaming file: %d\n", result);
            }
        } else if (strcmp(arg1, "moveFile") == 0) {
            if (num_args < 3) {
                printf("Usage: moveFile <src> <destDir>\n");
                continue;
            }
            int result = moveFile(arg2, arg3);
            if (result == SUCCESS) {
                printf("File '%s' moved to '%s'\n", arg2, arg3);
            } else {
                printf("Error moving file: %d\n", result);
            }
        } else if (strcmp(arg1, "list") == 0 || strcmp(arg1, "ls") == 0) {
            int result = list_directory(NULL);
            if (result != SUCCESS) {
                printf("Error listing directory: %d\n", result);
            }
        } else if (strcmp(arg1, "writeFile") == 0) {
            if (num_args < 3) {
                printf("Usage: writeFile <fd> <data>\n");
                continue;
            }
            int fd = atoi(arg2);
            int result = writeFile(fd, arg3, strlen(arg3));
            if (result >= 0) {
                printf("Wrote %d bytes to file\n", result);
            } else {
                printf("Error writing file: %d\n", result);
            }
        } else if (strcmp(arg1, "readFile") == 0) {
            if (num_args < 3) {
                printf("Usage: readFile <fd> <size>\n");
                continue;
            }
            int fd = atoi(arg2);
            int size = atoi(arg3);
            char buffer[4096];
            int result = readFile(fd, buffer, size);
            if (result >= 0) {
                buffer[result] = '\0';
                printf("Read %d bytes: %s\n", result, buffer);
            } else {
                printf("Error reading file: %d\n", result);
            }
        } else if (strcmp(arg1, "deleteFile") == 0) {
            if (num_args < 2) {
                printf("Usage: deleteFile <name>\n");
                continue;
            }
            int result = deleteFile(arg2);
            if (result == SUCCESS) {
                printf("File '%s' deleted\n", arg2);
            } else {
                printf("Error deleting file: %d\n", result);
            }
        } else if (strcmp(arg1, "pwd") == 0) {
            DirNode* current = get_current_directory();
            if (current != NULL) {
                printf("Current directory: %s\n", get_absolute_path(current));
            } else {
                printf("Error getting current directory\n");
            }
        } else {
            printf("Unknown command: %s\n", arg1);
        }
    }
    
    printf("Cleaning up...\n");
    cleanup_disk();
    printf("Goodbye!\n");
    
    return 0;
}
