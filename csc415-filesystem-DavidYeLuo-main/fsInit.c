/**************************************************************
* Class:  CSC-415-01 Fall 2021
* Names: Christian Francisco, David Ye Luo, Marc Castro, Rafael Sunico
* Student IDs: 920603057, 917051959, 921720147, 920261261
* GitHub Name: csc415-filesystem-DavidYeLuo
* Group Name: Segmentation Fault
* Project: Basic File System
*
* File: fsInit.c
*
* Description: File system management.
**************************************************************/


#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#include "fsLow.h"
#include "mfs.h"
#include "vcb.h"
#include "fsFree.h"
#include "dirEntry.h"

vcb* sffs_vcb;

int initFileSystem (uint64_t numberOfBlocks, uint64_t blockSize)
	{
	printf ("Initializing File System with %ld blocks with a block size of %ld\n", numberOfBlocks, blockSize);
	/* TODO: Add any code you need to initialize your file system. */

	sffs_vcb = malloc(blockSize);
	LBAread(sffs_vcb, 1, 0);

	if (strncmp(sffs_vcb->disk_sig, "sffs", 4) == 0) { // segmentation fault file system
		// signature matches

		printf("debug: vcb already formatted\n");

		// load FAT linked list into memory
		if (reloadFreeSpace(sffs_vcb) != 0) {
			printf("! - Failed to load free space\n");
			return 1;
		}

		cwd_location = sffs_vcb->root_location;

	} else {
		// signature does not match, initialise

		strcpy(sffs_vcb->disk_sig, "sffs");
		sffs_vcb->block_count = numberOfBlocks;
		sffs_vcb->block_size = blockSize;
		sffs_vcb->vcb_location = 0;

		//printf("debug: initializing vcb\n");
		//printf("block_count: %ld\n", sffs_vcb->block_count);
		//printf("block_size: %ld\n", sffs_vcb->block_size);
		//printf("vcb_location: %ld\n", sffs_vcb->vcb_location);

		uint64_t fat_location = initFreeSpace(sffs_vcb);
		if (fat_location == -1) {
			printf("! - LBAwrite failed to write FAT\n");
			return 1;
		}

		sffs_vcb->fat_location = fat_location;

		//printf("debug: initializing root\n");
		uint64_t root_location = createDir(0);
		if (root_location == -1) {
			printf("! - failed to initialise root\n");
			return 1;
		}

		sffs_vcb->root_location = root_location;
		sffs_vcb->root_size = (sizeof(dir_entry) * DE_CAPACITY)/blockSize + 1;
		sffs_vcb->fs_start_location = 2 + sffs_vcb->fat_size + sffs_vcb->root_size;

		//printf("root_location: %ld\n", sffs_vcb->root_location);
		//printf("root_size: %ld\n", sffs_vcb->root_size);
		//printf("fs_start_location: %ld\n", sffs_vcb->fs_start_location);
	}
	
	if (LBAwrite(sffs_vcb, 1, 0) != 1)
		{
			printf("! - LBAwrite failed to write VCB\n");
			return 1; // failed to write to disk
		}

	return 0; // TODO: change this to 0 when vcb can be correctly formatted
	}
	
	
void exitFileSystem ()
	{
	printf ("System exiting\n");
	}

void initVCB() {

}