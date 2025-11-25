File Management System Project
CS149 - Operating Systems
Team Members: Zander Gay, Johnathon, Alvin

in src are the C files with implementation and the headers dir
in the headers directory write the method signatures only

the scripts directory is for any .sh script files we may need to run

in the docs folder is where this readme and any other documentation can be written

## How to Build and Run

### Building the Project

To compile the project, use the Makefile:

```bash
make clean    # Clean previous builds
make          # Build the project
```

This will create the executable at `bin/fms`.

### Running the Program

To start the File Management System:

```bash
./bin/fms
```

The program will initialize the virtual disk (32 MiB) and display an interactive menu with available commands.

### Example Usage

```
createFile test.txt          # Create a new file
openFile test.txt 1           # Open file for writing (mode: 0=read, 1=write, 2=append)
writeFile 0 "Hello World"    # Write data to file (0 is the file descriptor)
closeFile 0                   # Close the file
openFile test.txt 0           # Open file for reading
readFile 0 11                # Read 11 bytes
closeFile 0                   # Close the file
mkDir mydir                   # Create a directory
cd mydir                      # Change directory
list                          # List current directory contents
pwd                           # Print working directory
searchFile test.txt           # Search and display file information
chmod test.txt rwx            # Change file permissions
renameFile test.txt new.txt   # Rename a file
moveFile new.txt mydir        # Move file to another directory
quit                          # Exit the program
```

### Available Commands

- **File Operations**: `createFile`, `openFile`, `closeFile`, `searchFile`, `renameFile`, `moveFile`, `deleteFile`
- **Directory Operations**: `mkDir`, `cd`, `list`, `pwd`
- **I/O Operations**: `writeFile <fd> <data>`, `readFile <fd> <size>`
- **Permissions**: `chmod <name> <permissions>` (e.g., `rwx` or `7`)
- **Exit**: `quit` or `q`