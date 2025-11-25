#ifndef PERMISSIONS_H
#define PERMISSIONS_H

#include "common.h"

// Permission operations
int chmod(const char* name, uint16_t permissions);
uint16_t parse_permissions(const char* perm_string);
void print_permissions(uint16_t permissions);

#endif