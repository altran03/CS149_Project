# File Management System - Implementation Summary

## Overview
This implementation provides a complete File Management System (FMS) that simulates core OS-level file handling operations. The system includes a 32 MiB virtual hard disk, directory tree structure, file control blocks, and all required file/directory operations.

## Data Structures

### File Control Block (FCB) - 64 bytes
- `uint16_t owner_id` - 16-bit owner/user ID
- `uint16_t permissions` - 16-bit access permissions (rwx)
- `uint16_t direct_blocks[18]` - 18 direct memory block pointers
- `uint16_t indirect_block_1` - 1st level indirect block
- `uint16_t indirect_block_2` - 2nd level indirect block
- `uint16_t file_type` - File type (regular or directory)
- `uint32_t size` - File size in bytes
- `uint32_t access_time` - Last access time
- `uint32_t modify_time` - Last modification time
- `uint32_t change_time` - Last metadata change time

### Hard Disk Structure
- **Total Size**: 32 MiB (16,384 blocks × 2,048 bytes)
- **Block Layout**:
  - Block 0: SuperBlock
  - Block 1: Free Bit Array
  - Blocks 2-65: FCB Storage (64 FCBs)
  - Block 66: Root Directory Block
  - Block 67+: Data Blocks

### Directory Tree
- Hierarchical directory structure with parent pointers
- Hash table-based directory entries for efficient lookup
- Directory nodes cached by FCB index for fast navigation

### File Descriptor Table
- Tracks open files with:
  - FCB pointer
  - Mode (read/write/append)
  - Reference count
  - Current offset

## Implemented Functions

### File Operations
- `createFile(name)` - Create a new file
- `openFile(name, operation)` - Open file with specified mode
- `closeFile(fd)` - Close an open file descriptor
- `searchFile(name)` - Search and display file information
- `renameFile(oldName, newName)` - Rename a file
- `moveFile(src, dir)` - Move file to another directory
- `writeFile(fd, data, size)` - Write data to file
- `readFile(fd, buffer, size)` - Read data from file
- `deleteFile(name)` - Delete a file

### Directory Operations
- `mkDir(name)` - Create a new directory
- `cd(path)` - Change current directory
- `list_directory(path)` - List directory contents
- `remove_directory(name)` - Remove a directory

### Permission Operations
- `chmod(name, permissions)` - Change file permissions
- `parse_permissions(perm_string)` - Parse permission string
- `print_permissions(permissions)` - Display permissions

## Building and Running

### Build
```bash
make clean
make
```

### Run
```bash
./bin/fms
```

### Interactive Commands
The system provides an interactive menu with the following commands:
1. `createFile <name>`
2. `openFile <name> <mode>` (0=read, 1=write, 2=append)
3. `closeFile <fd>`
4. `searchFile <name>`
5. `mkDir <name>`
6. `cd <path>`
7. `chmod <name> <permissions>` (e.g., rwx or 7)
8. `renameFile <oldName> <newName>`
9. `moveFile <src> <destDir>`
10. `list` - List current directory
11. `writeFile <fd> <data>`
12. `readFile <fd> <size>`
13. `deleteFile <name>`
14. `pwd` - Print working directory
15. `quit` - Exit

## Features

### Block Management
- Free block tracking using bit array
- Automatic block allocation/deallocation
- Support for direct and indirect blocks (indirect blocks not fully implemented)

### Directory Navigation
- Absolute and relative path support
- `.` and `..` directory navigation
- Directory tree maintained in memory with caching

### File I/O
- Sequential read/write operations
- Append mode support
- File offset tracking
- Multiple concurrent file opens (reference counting)

### Permissions
- Read, Write, Execute permissions
- Owner-based permission checking
- Permission string parsing (rwx format or octal)

## Limitations and Future Enhancements

1. **Indirect Blocks**: Currently only direct blocks are fully implemented. Indirect blocks are allocated but not used for data storage.

2. **Persistence**: The file system is in-memory only. Adding disk persistence would require serialization to a file.

3. **Directory Tree**: Directory nodes are created on-demand. A more robust implementation would maintain a complete tree structure.

4. **Error Handling**: Some error cases could be more descriptive.

5. **Optional Features**: Copying files and JSON export/backup mentioned in requirements are not yet implemented.

## File Structure

```
src/
├── main.c                    - Main program and interactive interface
├── disk.c                    - Disk initialization and block management
├── utils.c                   - Utility functions (hashing, FCB management)
├── directory_operations.c    - Directory operations implementation
├── file_operations.c         - File operations implementation
├── permissions.c             - Permission management
└── headers/
    ├── common.h              - Common structures and constants
    ├── file_operations.h     - File operation declarations
    ├── directory_operations.h - Directory operation declarations
    ├── permissions.h         - Permission operation declarations
    └── utils.h               - Utility function declarations
```

## Testing

The system can be tested interactively using the provided menu interface. Example session:

```
createFile test.txt
openFile test.txt 1
writeFile 0 "Hello, World!"
closeFile 0
openFile test.txt 0
readFile 0 13
closeFile 0
```

This creates a file, writes data to it, then reads it back.

