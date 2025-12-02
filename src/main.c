#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "headers/common.h"
#include "headers/file_operations.h"
#include "headers/directory_operations.h"
#include "headers/utils.h"

// External references to globals defined in dev.c
extern SessionConfig *session_config;
extern uint8_t HARD_DISK[BLOCK_NUM][BLOCK_SIZE_BYTES];

// Helper function: List directory contents
void list_directory(uint16_t dir_inode)
{
    // Get the directory's inode
    uint16_t inode_block = INODE_START + (dir_inode / 32);
    uint16_t inode_offset = (dir_inode % 32) * sizeof(Inode);
    Inode *dir_inode_ptr = (Inode *)(HARD_DISK[inode_block] + inode_offset);
    
    // Check if it's actually a directory
    if ((dir_inode_ptr->flags & 2) == 0) {
        printf("Error: Not a directory\n");
        return;
    }
    
    // Get the directory's first data block
    uint16_t dir_data_block = dir_inode_ptr->directBlocks[0];
    if (dir_data_block == 0) {
        printf("(empty directory)\n");
        return;
    }
    
    uint8_t *dir_data = HARD_DISK[dir_data_block];
    uint16_t dir_size = dir_inode_ptr->file_size;
    uint16_t offset = 0;
    int count = 0;
    
    // Iterate through directory entries
    while (offset < dir_size) {
        DirectoryEntry *entry = get_directory_entry_at_offset(dir_data, offset);
        
        // Skip . and .. entries
        if (entry->name_length > 0 && 
            !(entry->name_length == 1 && entry->name[0] == '.') &&
            !(entry->name_length == 2 && entry->name[0] == '.' && entry->name[1] == '.')) {
            
            // Get entry's inode to check type
            uint16_t entry_inode = entry->inode_number;
            uint16_t entry_inode_block = INODE_START + (entry_inode / 32);
            uint16_t entry_inode_offset = (entry_inode % 32) * sizeof(Inode);
            Inode *entry_inode_ptr = (Inode *)(HARD_DISK[entry_inode_block] + entry_inode_offset);
            
            // Print entry info
            if ((entry_inode_ptr->flags & 2) != 0) {
                printf("  [DIR]  %.*s (inode: %d)\n", entry->name_length, entry->name, entry_inode);
            } else {
                printf("  [FILE] %.*s (inode: %d, size: %u bytes)\n", 
                       entry->name_length, entry->name, entry_inode, entry_inode_ptr->file_size);
            }
            count++;
        }
        
        // Move to next entry
        offset = get_next_directory_entry_offset(dir_data, offset, dir_size);
    }
    
    if (count == 0) {
        printf("(empty directory)\n");
    }
}

// Interactive shell for file system demo
int interactive_shell()
{
    char input[1024];
    char command[64];
    char arg1[256];
    char arg2[256];
    int fd;
    int result;
    
    printf("========================================\n");
    printf("  Interactive File System Shell\n");
    printf("========================================\n");
    printf("Type 'help' for available commands\n");
    printf("Type 'exit' or 'quit' to exit\n\n");
    
    while (1) {
        // Print prompt
        printf("fs> ");
        fflush(stdout);
        
        // Read input
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("\n");
            break;
        }
        
        // Remove newline
        input[strcspn(input, "\n")] = 0;
        
        // Skip empty lines
        if (strlen(input) == 0) {
            continue;
        }
        
        // Parse command
        int parsed = sscanf(input, "%63s %255s %255s", command, arg1, arg2);
        
        if (strcmp(command, "help") == 0 || strcmp(command, "?") == 0) {
            printf("\nAvailable commands:\n");
            printf("  mkdir <dir>          - Create a directory\n");
            printf("  create <file>         - Create a file\n");
            printf("  ls [dir]              - List directory contents (default: current directory)\n");
            printf("  cd <dir>              - Change directory\n");
            printf("  pwd                   - Print current working directory\n");
            printf("  open <file> [mode]     - Open a file (mode: r=read, w=write, rw=readwrite)\n");
            printf("  close <fd>            - Close a file descriptor\n");
            printf("  read <fd> [bytes]      - Read from file (default: 1024 bytes)\n");
            printf("  write <fd> <text>      - Write text to file\n");
            printf("  search <pattern> [dir] - Search for files by name pattern\n");
            printf("  stat <file>            - Show file information\n");
            printf("  help                   - Show this help message\n");
            printf("  exit/quit              - Exit the shell\n\n");
            
        } else if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
            printf("Goodbye!\n");
            break;
            
        } else if (strcmp(command, "pwd") == 0) {
            printf("%s\n", session_config->current_working_dir);
            
        } else if (strcmp(command, "mkdir") == 0) {
            if (parsed < 2) {
                printf("Usage: mkdir <directory_name>\n");
                continue;
            }
            uint16_t dir_inode = create_directory(arg1);
            if (dir_inode != 0) {
                printf("Created directory '%s' (inode: %d)\n", arg1, dir_inode);
            } else {
                printf("Failed to create directory '%s'\n", arg1);
            }
            
        } else if (strcmp(command, "create") == 0) {
            if (parsed < 2) {
                printf("Usage: create <file_name>\n");
                continue;
            }
            result = create_file(arg1);
            if (result == SUCCESS) {
                printf("Created file '%s'\n", arg1);
            } else {
                printf("Failed to create file '%s' (error: %d)\n", arg1, result);
            }
            
        } else if (strcmp(command, "ls") == 0) {
            if (parsed >= 2) {
                // List specified directory
                uint16_t target_inode;
                result = traverse_path(arg1, &target_inode);
                if (result == SUCCESS) {
                    printf("Contents of '%s':\n", arg1);
                    list_directory(target_inode);
                } else {
                    printf("Error: Cannot access '%s' (error: %d)\n", arg1, result);
                }
            } else {
                // List current directory
                printf("Contents of '%s':\n", session_config->current_working_dir);
                list_directory(session_config->current_dir_inode);
            }
            
        } else if (strcmp(command, "cd") == 0) {
            if (parsed < 2) {
                printf("Usage: cd <directory>\n");
                continue;
            }
            uint16_t target_inode;
            result = traverse_path(arg1, &target_inode);
            if (result == SUCCESS) {
                // Check if it's a directory
                uint16_t inode_block = INODE_START + (target_inode / 32);
                uint16_t inode_offset = (target_inode % 32) * sizeof(Inode);
                Inode *inode = (Inode *)(HARD_DISK[inode_block] + inode_offset);
                
                if ((inode->flags & 2) != 0) {
                    session_config->current_dir_inode = target_inode;
                    // Update current working directory path
                    if (arg1[0] == '/') {
                        strncpy(session_config->current_working_dir, arg1, MAX_PATH_LENGTH - 1);
                    } else {
                        // Relative path
                        if (strcmp(session_config->current_working_dir, "/") == 0) {
                            snprintf(session_config->current_working_dir, MAX_PATH_LENGTH, "/%s", arg1);
                        } else {
                            snprintf(session_config->current_working_dir, MAX_PATH_LENGTH, "%s/%s", 
                                    session_config->current_working_dir, arg1);
                        }
                    }
                } else {
                    printf("Error: '%s' is not a directory\n", arg1);
                }
            } else {
                printf("Error: Cannot change to '%s' (error: %d)\n", arg1, result);
            }
            
        } else if (strcmp(command, "open") == 0) {
            if (parsed < 2) {
                printf("Usage: open <file> [mode]\n");
                continue;
            }
            uint16_t mode = O_RDONLY;
            if (parsed >= 3) {
                if (strcmp(arg2, "r") == 0 || strcmp(arg2, "read") == 0) {
                    mode = O_RDONLY;
                } else if (strcmp(arg2, "w") == 0 || strcmp(arg2, "write") == 0) {
                    mode = O_WRONLY;
                } else if (strcmp(arg2, "rw") == 0 || strcmp(arg2, "readwrite") == 0) {
                    mode = O_RDWR;
                } else if (strcmp(arg2, "c") == 0 || strcmp(arg2, "create") == 0) {
                    mode = O_RDWR | O_CREAT;
                }
            }
            fd = fs_open(arg1, mode);
            if (fd >= 0) {
                printf("Opened '%s' with fd = %d\n", arg1, fd);
            } else {
                printf("Failed to open '%s' (error: %d)\n", arg1, fd);
            }
            
        } else if (strcmp(command, "close") == 0) {
            if (parsed < 2) {
                printf("Usage: close <file_descriptor>\n");
                continue;
            }
            fd = atoi(arg1);
            result = fs_close(fd);
            if (result == SUCCESS) {
                printf("Closed file descriptor %d\n", fd);
            } else {
                printf("Failed to close fd %d (error: %d)\n", fd, result);
            }
            
        } else if (strcmp(command, "read") == 0) {
            if (parsed < 2) {
                printf("Usage: read <file_descriptor> [bytes]\n");
                continue;
            }
            fd = atoi(arg1);
            size_t bytes_to_read = 1024; // Default
            if (parsed >= 3) {
                bytes_to_read = (size_t)atoi(arg2);
            }
            
            char *read_buffer = malloc(bytes_to_read + 1);
            if (read_buffer == NULL) {
                printf("Error: Memory allocation failed\n");
                continue;
            }
            
            result = fs_read(fd, read_buffer, bytes_to_read);
            if (result >= 0) {
                read_buffer[result] = '\0'; // Null terminate
                printf("Read %d bytes:\n", result);
                printf("---\n%s\n---\n", read_buffer);
            } else {
                printf("Failed to read from fd %d (error: %d)\n", fd, result);
            }
            free(read_buffer);
            
        } else if (strcmp(command, "write") == 0) {
            if (parsed < 2) {
                printf("Usage: write <file_descriptor> <text>\n");
                continue;
            }
            fd = atoi(arg1);
            
            // Find the text to write (everything after "write <fd> ")
            char *text_start = input;
            // Skip past "write "
            text_start = strchr(text_start, ' ');
            if (text_start) {
                text_start++; // Skip space
                // Skip past fd
                while (*text_start && *text_start != ' ') text_start++;
                // Skip spaces
                while (*text_start == ' ') text_start++;
            }
            
            if (text_start == NULL || strlen(text_start) == 0) {
                printf("Error: No text to write\n");
                continue;
            }
            
            result = fs_write(fd, text_start, strlen(text_start));
            if (result >= 0) {
                printf("Wrote %d bytes to fd %d\n", result, fd);
            } else {
                printf("Failed to write to fd %d (error: %d)\n", fd, result);
            }
            
        } else if (strcmp(command, "search") == 0) {
            if (parsed < 2) {
                printf("Usage: search <pattern> [directory]\n");
                continue;
            }
            const char *search_dir = (parsed >= 3) ? arg2 : session_config->current_working_dir;
            char results[100][MAX_PATH_LENGTH];
            int num_results = search_files_by_name(search_dir, arg1, results, 100);
            if (num_results >= 0) {
                printf("Found %d file(s) matching '%s' in '%s':\n", num_results, arg1, search_dir);
                for (int i = 0; i < num_results; i++) {
                    printf("  %d. %s\n", i + 1, results[i]);
                }
            } else {
                printf("Search failed (error: %d)\n", num_results);
            }
            
        } else if (strcmp(command, "stat") == 0) {
            if (parsed < 2) {
                printf("Usage: stat <file>\n");
                continue;
            }
            uint16_t target_inode;
            result = traverse_path(arg1, &target_inode);
            if (result == SUCCESS) {
                uint16_t inode_block = INODE_START + (target_inode / 32);
                uint16_t inode_offset = (target_inode % 32) * sizeof(Inode);
                Inode *inode = (Inode *)(HARD_DISK[inode_block] + inode_offset);
                
                printf("File: %s\n", arg1);
                printf("  Inode: %d\n", target_inode);
                printf("  Type: %s\n", (inode->flags & 2) ? "Directory" : "File");
                printf("  Size: %u bytes\n", inode->file_size);
                printf("  Permissions: %o\n", inode->permissions);
                printf("  Owner: %d\n", inode->ownerID);
                printf("  Created: %s", ctime(&inode->ctime));
                printf("  Modified: %s", ctime(&inode->mtime));
                printf("  Accessed: %s", ctime(&inode->time));
            } else {
                printf("Error: Cannot stat '%s' (error: %d)\n", arg1, result);
            }
            
        } else {
            printf("Unknown command: %s\n", command);
            printf("Type 'help' for available commands\n");
        }
    }
    
    return 0;
}

int main()
{
    printf("========================================\n");
    printf("  File System Demo\n");
    printf("========================================\n\n");
    
    // Initialize session
    session_config = (SessionConfig *)malloc(sizeof(SessionConfig));
    session_config->uid = 0;
    strcpy(session_config->current_working_dir, "/");
    session_config->current_dir_inode = 0;
    session_config->show_hidden_files = false;
    session_config->verbose_mode = true;

    printf("File System Configuration:\n");
    printf("  Blocks: %d\n", BLOCK_NUM);
    printf("  Block Size: %d bytes\n", BLOCK_SIZE_BYTES);
    printf("  Inode Size: %ld bytes\n", sizeof(Inode));
    printf("  Total Inodes: %ld\n", (INODE_END - INODE_START + 1) * (BLOCK_SIZE_BYTES / sizeof(Inode)));
    printf("  Data Blocks: %d\n", DATA_END - DATA_START + 1);
    printf("\n");

    // Initialize file system
    reset_hard_disk();
    create_root_directory();
    printf("✓ Root directory created\n\n");
    
    // Start interactive shell
    return interactive_shell();
}
