#ifndef FILE_OPERATIONS_H
#define FILE_OPERATIONS_H

#include "common.h"

// File operations
int createFile(const char* name);
int openFile(const char* name, int operation);
int closeFile(int fd);
int searchFile(const char* name);
int renameFile(const char* oldName, const char* newName);
int moveFile(const char* src, const char* dir);
int writeFile(int fd, const char* data, int size);
int readFile(int fd, char* buffer, int size);
int deleteFile(const char* name);

#endif