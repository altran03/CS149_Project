#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>

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

#endif