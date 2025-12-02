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
#include "headers/common.h"

/*
16 bits is enough for number of blocks 2^16=65536>16384, each block is 2KiB
uint8_t, one byte is 8 bits
*/
uint8_t hard_disk[BLOCK_NUM][BLOCK_SIZE_BYTES]; 
int main(){
	
	printf("Size of Inode is %ld bytes\n", sizeof(Inode)); //64
    printf("Number of Inodes is %d\n", (INODE_END-INODE_START+1)*32); //16384
    printf("Number of data blocks is %d\n", DATA_END-DATA_START+1); //15870
    return 0;
	
}
