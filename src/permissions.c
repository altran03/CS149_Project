#include "headers/permissions.h"
#include "headers/directory_operations.h"
#include "headers/utils.h"
#include "headers/common.h"
#include <string.h>
#include <stdio.h>

// Change file permissions
int chmod(const char* name, uint16_t permissions) {
    if (disk == NULL || name == NULL) {
        return ERROR_INVALID_INPUT;
    }

    DirNode* current = get_current_directory();
    if (current == NULL) {
        return ERROR_INVALID_INPUT;
    }

    DirEntry* entry = find_entry_in_directory(current, name);
    if (entry == NULL) {
        return ERROR_FILE_NOT_FOUND;
    }

    FCB* fcb = get_fcb(entry->fcb_index);
    if (fcb == NULL) {
        return ERROR_INVALID_INPUT;
    }

    // Check if user has permission to change (simplified - owner check)
    if (fcb->owner_id != getuid()) {
        return ERROR_PERMISSION_DENIED;
    }

    // Update permissions
    fcb->permissions = permissions & 0x07; // Only use lower 3 bits (rwx)
    update_fcb_times(fcb, 0, 0, 1);

    return SUCCESS;
}

// Parse permission string (e.g., "rwx", "755")
uint16_t parse_permissions(const char* perm_string) {
    if (perm_string == NULL) {
        return 0;
    }

    uint16_t perms = 0;

    // Check if it's numeric (e.g., "755")
    if (perm_string[0] >= '0' && perm_string[0] <= '7') {
        int num = 0;
        sscanf(perm_string, "%o", &num);
        return num & 0x07;
    }

    // Parse string format (e.g., "rwx", "rw-")
    for (int i = 0; i < 3 && perm_string[i] != '\0'; i++) {
        switch (perm_string[i]) {
            case 'r':
            case 'R':
                perms |= PERM_READ;
                break;
            case 'w':
            case 'W':
                perms |= PERM_WRITE;
                break;
            case 'x':
            case 'X':
                perms |= PERM_EXEC;
                break;
            case '-':
                break;
            default:
                return 0; // Invalid character
        }
    }

    return perms;
}

// Print permissions in human-readable format
void print_permissions(uint16_t permissions) {
    printf("%c%c%c",
           (permissions & PERM_READ) ? 'r' : '-',
           (permissions & PERM_WRITE) ? 'w' : '-',
           (permissions & PERM_EXEC) ? 'x' : '-');
}

