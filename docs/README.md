File Management System Project
CS149 - Operating Systems
Team Members: Zander Gay, Johnathon, Alvin

in src are the C files with implementation and the headers dir
in the headers directory write the method signatures only

the scripts directory is for any .sh script files we may need to run

in the docs folder is where this readme and any other documentation can be written

Running:
   gcc src/main.c src/directory_operations.c src/file_operations.c src/utils.c -I src/headers -o filesystem
   ./filesystem