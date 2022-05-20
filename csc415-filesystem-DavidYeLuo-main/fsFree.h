/**************************************************************
* Class:  CSC-415-01 Fall 2021
* Names: Christian Francisco, David Ye Luo, Marc Castro, Rafael Sunico
* Student IDs: 920603057, 917051959, 921720147, 920261261
* GitHub Name: csc415-filesystem-DavidYeLuo
* Group Name: Segmentation Fault
* Project: Basic File System
*
* File: fsFree.h
*
* Description: Free space management definitions.
**************************************************************/
#include "vcb.h"

uint64_t initFreeSpace(vcb *sffs_vcb);

int reloadFreeSpace();

uint64_t allocateFreeSpace(uint64_t block_count);

// This behaves the same as LBAwrite, but splits the LBAwrite calls for fragmented FAT entries.
// Do not use this if the blocks needed to write is different to the number of blocks already allocated,
// instead use releaseFreeSpace and allocateFreeSpace, THEN use overwriteAllocated.
// @param buffer, the raw bytes to write into volume
// @param block_count, the AMOUNT of blocks to write
// @param block_location, the STARTING block of where to write
// @return blocks_written, the actual amount of blocks written.
uint64_t overwriteAllocated(void *buffer, uint64_t block_count, uint64_t block_location);

// This behaves the same as LBAread, but splits the LBAread calls for fragmented FAT entries.
// @param buffer, buffer to hold the raw bytes read from volume
// @param block_count, the AMOUNT of blocks to read
// @param block_location, the STARTING block of where to read
// @return blocks_read, the actual amount of blocks read.
uint64_t readAllocated(void *buffer, uint64_t block_count, uint64_t block_location);

// This traverses FAT from block_location and marks subsequent blocks as free
// @param block_location, the STARTING block of where to start releasing blocks
// @return 0 for success, -1 for error
int releaseFreeSpace(uint64_t block_location);
