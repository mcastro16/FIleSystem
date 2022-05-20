/**************************************************************
 * Class:  CSC-415-01 Fall 2021
 * Names: Christian Francisco, David Ye Luo, Marc Castro, Rafael Sunico
 * Student IDs: 920603057, 917051959, 921720147, 920261261
 * GitHub Name: csc415-filesystem-DavidYeLuo
 * Group Name: Segmentation Fault
 * Project: Basic File System
 *
 * File: b_io.c
 *
 * Description: Basic File System - Key File I/O Operations
 *
 **************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h> // for malloc
#include <string.h> // for memcpy
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "b_io.h"
#include "mfs.h"
#include "dirEntry.h"
#include "vcb.h"
#include "fsFree.h"

#define MAXFCBS 20
#define B_CHUNK_SIZE 512
#define DEFAULT_FILE_BLOCK_COUNT 10
#define DEFAULT_FILE_BLOCK_EXTEND_SIZE 5

typedef struct b_fcb
{
	/** TODO add al the information you need in the file control block **/
	char *buf;	// holds the open file buffer
	int index;	// holds the current position in the buffer
	int buflen; // holds how many valid bytes are in the buffer

	dir_entry *dirEntry; // This is passed in b_open()
	int flags;
	uint64_t currentBlockNum;
	bool isDirty; // Is the buffer uncommitted?
} b_fcb;

b_fcb fcbArray[MAXFCBS];

int startup = 0; // Indicates that this has not been initialized

// Method to initialize our file system
void b_init()
{
	// init fcbArray to all free
	for (int i = 0; i < MAXFCBS; i++)
	{
		fcbArray[i].buf = NULL; // indicates a free fcbArray
		fcbArray[i].index = 0;
		fcbArray[i].buflen = 0;
		fcbArray[i].dirEntry = NULL;
		fcbArray[i].flags = 0;
		fcbArray[i].currentBlockNum = 0;
		fcbArray[i].isDirty = false;
	}

	startup = 1;
}

// Method to get a free FCB element
b_io_fd b_getFCB()
{
	for (int i = 0; i < MAXFCBS; i++)
	{
		if (fcbArray[i].buf == NULL)
		{
			return i; // Not thread safe (But do not worry about it for this assignment)
		}
	}
	return (-1); // all in use
}

// Helpers

// getBlockXofFile is a helper function for b_read() (and b_write()?)
// Encapsulating the traverse the FAT process
// It is basically readAllocated but with relative file location
// @param fcb, file control block
// @param buffer, get result
// @param relativeFileBlockNum offset relative to the starting file location
// @param block_count, the amount of blocks to read
// @return number of blocks read
uint64_t getBlockXofFile(b_fcb *fcb, uint64_t relativeFileBlockNum, uint64_t block_count)
{
	if (fcb == NULL)
	{
		printf("[ERROR] FCB is NULL in getBlockXofFile.");
	}

    // Translate from relative position to absolute position
	uint64_t fileAbsolutePosition = (fcb->dirEntry->location) / (sffs_vcb->block_size);
    fileAbsolutePosition += relativeFileBlockNum;

    // Read block
	uint64_t blocksRead = readAllocated(fcb->buf, block_count, fileAbsolutePosition);
	return blocksRead;
}

// putBlockXofFile is a helper function for b_read() (and b_write()?)
// encapsulating the traverse the fat process
// it is basically overwriteallocated but with relative file location
// @param fcb, file control block
// @param buffer, buffer to write to the blocks
// @param relativefileblocknum offset relative to the starting file location
// @param block_count, the amount of blocks to read
// @return number of blocks wrote
uint64_t putBlockXofFile(b_fcb *fcb, uint64_t relativeFileBlockNum, uint64_t block_count)
{
	if (fcb == NULL)
	{
		printf("[ERROR] FCB is NULL in putBlockXofFile.");
	}

    // Translate from relative position to absolute position
	uint64_t fileAbsolutePosition = (fcb->dirEntry->location) / (sffs_vcb->block_size);
    fileAbsolutePosition += relativeFileBlockNum;

    // Write Block
	uint64_t blockWritten = overwriteAllocated(fcb->buf, block_count, fileAbsolutePosition);
	return blockWritten;
}

// Interface to open a buffered file
// Modification of interface for this assignment, flags match the Linux flags for open
// O_RDONLY, O_WRONLY, or O_RDWR
b_io_fd b_open(char *filename, int flags)
{
	b_io_fd returnFd;

	if (startup == 0)
		b_init(); // Initialize our system

	// validate path, get parent directory and last token
	uint64_t blocksNeeded = (DIR_ENTRY_LIST_SIZE / sffs_vcb->block_size) + 1;
	dir_entry *temp_dir = malloc(blocksNeeded * sffs_vcb->block_size);
	char *last_token = malloc(256);

	bool valid = pathParser(filename, EXIST_FILE, temp_dir, last_token);

	if (!valid)
	{
		if (flags | O_CREAT)
		{
			uint64_t start_block = allocateFreeSpace(DEFAULT_FILE_BLOCK_COUNT);

			int dir_entry_index = -1;
			for (int i = 2; i < DE_CAPACITY; i++)
			{
				if (strcmp(temp_dir[i].name, "\0") == 0)
				{
					dir_entry_index = i;
					break;
				}
			}

			if (dir_entry_index == -1)
			{
				printf("[ERROR] b_open, directory is full\n");
				free(temp_dir);
				free(last_token);
				return -1;
			}

			strcpy(temp_dir[dir_entry_index].name, last_token);
			temp_dir[dir_entry_index].isDir = false;
			temp_dir[dir_entry_index].location = start_block;
			temp_dir[dir_entry_index].size = DEFAULT_FILE_BLOCK_COUNT;
			temp_dir[dir_entry_index].reclen = 0;
			// TODO: Add create, lastMod, lastAccess time

			char *buf = malloc(DEFAULT_FILE_BLOCK_COUNT * sffs_vcb->block_size);
			uint64_t blocks_written = overwriteAllocated(buf, DEFAULT_FILE_BLOCK_COUNT, start_block);
			free(buf);
			if (blocks_written != DEFAULT_FILE_BLOCK_COUNT)
			{
				printf("[ERROR] b_io.c:137, overwriteAllocated failed\n");
				free(temp_dir);
				free(last_token);
				return -1;
			}

			uint64_t write_blocks_needed = temp_dir[0].size;
			blocks_written = overwriteAllocated(temp_dir, temp_dir[0].size, temp_dir[0].location);
			if (blocks_written != write_blocks_needed)
			{
				printf("[ERROR] b_io.c:145, overwriteAllocated failed\n");
				free(temp_dir);
				free(last_token);
				return -1;
			}
		}
		else
		{
			printf("[ERROR] b_open, file does not exist\n");
			free(temp_dir);
			free(last_token);
			return -1;
		}
	}

	returnFd = b_getFCB(); // get our own file descriptor
	if (returnFd == -1)
	{
		printf("[ERROR] b_open, all fcb are in use\n");
		free(temp_dir);
		free(last_token);
		return -1;
	}

	int dir_entry_index = -1;
	for (int i = 2; i < DE_CAPACITY; i++)
	{
		if (strcmp(temp_dir[i].name, last_token) == 0)
		{
			dir_entry_index = i;
			break;
		}
	}
	if (dir_entry_index == -1)
	{
		printf("[ERROR] b_open, dir_entry does not exist\n");
		free(temp_dir);
		free(last_token);
		return -1;
	}

	/*
	char *buf;	// holds the open file buffer
	int index;	// holds the current position in the buffer
	int buflen; // holds how many valid bytes are in the buffer

	dir_entry *dirEntry; // This is passed in b_open()
	int flags;
	uint64_t currentBlockNum;
	bool isDirty; // Is the buffer uncommitted?
	*/

	fcbArray[returnFd].buf = malloc(B_CHUNK_SIZE);
	fcbArray[returnFd].index = 0;
	fcbArray[returnFd].buflen = 0;
	fcbArray[returnFd].dirEntry = &temp_dir[dir_entry_index];
	fcbArray[returnFd].flags = flags;
	fcbArray[returnFd].currentBlockNum = 0;
	fcbArray[returnFd].isDirty = false;

	if (flags & O_TRUNC)
	{
		char *buf = malloc(fcbArray[returnFd].dirEntry->size * sffs_vcb->block_size);
		uint64_t blocks_written = overwriteAllocated(buf, fcbArray[returnFd].dirEntry->size, fcbArray[returnFd].dirEntry->location);
		free(buf);
		if (blocks_written != fcbArray[returnFd].dirEntry->size)
		{
			printf("[ERROR] b_io.c:196, overwriteAllocated failed\n");
			free(temp_dir);
			free(last_token);
			return -1;
		}

		uint64_t write_blocks_needed = temp_dir[0].size;
		blocks_written = overwriteAllocated(temp_dir, temp_dir[0].size, temp_dir[0].location);
		if (blocks_written != write_blocks_needed)
		{
			printf("[ERROR] b_io.c:204, overwriteAllocated failed\n");
			free(temp_dir);
			free(last_token);
			return -1;
		}
	}

	if (flags & O_APPEND)
	{
		b_seek(returnFd, fcbArray[returnFd].index, SEEK_END);
	}

	free(temp_dir);
	free(last_token);

	return (returnFd); // all set
}

// Interface to seek function
// @todo Check if the return value should be in bytes or in blocks
// @param fd, a file descriptor
// @param offset in bytes
// @param whence, values: SEEK_SET, SEEK_CUR, SEEK_END
// @return Offset displacement from the beginning of the file in bytes.
// 		Returns -1 if error
int b_seek(b_io_fd fd, off_t offset, int whence)
{
	if (startup == 0)
		b_init(); // Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
	{
		return (-1); // invalid file descriptor
	}

	int newPos;
	switch (whence)
	{
	case SEEK_SET:
		newPos = offset;
		break;
	case SEEK_CUR:
		newPos += offset;
		break;
	case SEEK_END:
		newPos = fcbArray[fd].dirEntry->reclen + offset - 1;
        break;
	default:
		printf("[ERROR] Whence not found/recognized.\n");
		return -1;
	}
	fcbArray[fd].index = newPos;

	return newPos;
}

// Interface to write function
// @param fd, file descriptor.
// @param *buffer, buffer to write to the disk.
// @param count, number of bytes to write.
// @return number of bytes write.
int b_write(b_io_fd fd, char *buffer, int count)
{
	if (startup == 0)
		b_init(); // Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
	{
		return (-1); // invalid file descriptor
	}

	//check if we have write permissions enabled
	if(!(fcbArray[fd].flags | O_RDWR || fcbArray[fd].flags | O_WRONLY)) {
        printf("[ERROR] does not have access to write\n");
		return -1;
	}


	// if user is asking for more bytes, then update size
	if ((fcbArray[fd].index + count) > fcbArray[fd].dirEntry->reclen)
	{
		// TODO: Allocate more space
	}

	int p1, p2, p3; // Bytes needed to be write
	int bytesRead;

	// calculate parts 1, 2, and 3 from above
	if (fcbArray[fd].index + count <= B_CHUNK_SIZE) // If 1 block is enough
	{
		p1 = B_CHUNK_SIZE - (fcbArray[fd].index % B_CHUNK_SIZE);
		p2 = 0;
		p3 = 0;
	}
	else
	{
		p1 = B_CHUNK_SIZE - (fcbArray[fd].index % B_CHUNK_SIZE);
        p2 = (fcbArray[fd].index + count) / B_CHUNK_SIZE;
        p3 = count % B_CHUNK_SIZE;
	}

    // starting bytes to call uint64_t putBlockXofFile
    int startBlock;
    int blocksNeeded;
    fcbArray[fd].buflen = 0;

    int buf_index = 0;

	// part 1 - what can be filled from existing buffer
	if (p1 > 0)
	{
        // Copy buffer to write to disk
        memcpy(fcbArray[fd].buf + buf_index, buffer + fcbArray[fd].index, p1); // TODO: reverse string copy

        // Transfer read buffer to display buffer
        startBlock = 0;
        blocksNeeded = 1;
        bytesRead = putBlockXofFile(&fcbArray[fd], startBlock, blocksNeeded);
		fcbArray[fd].index += bytesRead;
        buf_index += blocksNeeded;

        // Make sure that we read everything we need
        if(bytesRead != p1)
        {
            printf("[DEBUG]: Not all Bytes were read from getBlockXofFile.\n");
            if(bytesRead != 0)
            {
                fcbArray[fd].isDirty = true;
            }
            return bytesRead;
        }

        // Set Dirty buffer
        fcbArray[fd].isDirty = true;
	}
	// part 2 - filled direct in multiples of the block size
	if (p2 > 0)
	{
        // Copy buffer to write to disk
        memcpy(fcbArray[fd].buf + buf_index, buffer + fcbArray[fd].index, p2);

        // Transfer read buffer to display buffer
        startBlock = buf_index; // Should be 1
        blocksNeeded = B_CHUNK_SIZE;

        for(int i = 0; i < blocksNeeded; i++)
        {
            bytesRead = putBlockXofFile(&fcbArray[fd], startBlock, blocksNeeded);
            fcbArray[fd].index += bytesRead;
            buf_index += blocksNeeded * B_CHUNK_SIZE;
            startBlock++;

            // Make sure that we read everything we need
            if(bytesRead < sffs_vcb->block_size)
            {
                printf("[DEBUG]: Not all Bytes were read from getBlockXofFile.\n");
                return (p1 + bytesRead);
            }
        }
	}
	// part 3 - what can be filled from refilled buffer
	if (p3 > 0)
	{
        // Copy buffer to write to disk
        memcpy(fcbArray[fd].buf + buf_index, buffer + fcbArray[fd].index, p3);

        // Transfer read buffer to display buffer
        startBlock = buf_index;
        blocksNeeded = 1;
        bytesRead = putBlockXofFile(&fcbArray[fd], startBlock, blocksNeeded);
		fcbArray[fd].index += bytesRead;
        buf_index += B_CHUNK_SIZE;
	}

	// calculate returned bytes
	return (p1 + p2 + bytesRead);
}

// Interface to read a buffer

// Filling the callers request is broken into three parts
// Part 1 is what can be filled from the current buffer, which may or may not be enough
// Part 2 is after using what was left in our buffer there is still 1 or more block
//        size chunks needed to fill the callers request.  This represents the number of
//        bytes in multiples of the blocksize.
// Part 3 is a value less than blocksize which is what remains to copy to the callers buffer
//        after fulfilling part 1 and part 2.  This would always be filled from a refill
//        of our buffer.
//  +-------------+------------------------------------------------+--------+
//  |             |                                                |        |
//  | filled from |  filled direct in multiples of the block size  | filled |
//  | existing    |                                                | from   |
//  | buffer      |                                                |refilled|
//  |             |                                                | buffer |
//  |             |                                                |        |
//  | Part1       |  Part 2                                        | Part3  |
//  +-------------+------------------------------------------------+--------+
// @todo is the param count should be bytes or blocks? same with return?
// @param fd, file descriptor.
// @param *buffer, read result.
// @param count, number of bytes to read.
// @return number of bytes read.
int b_read(b_io_fd fd, char *buffer, int count)
{
	if (startup == 0)
		b_init(); // Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
	{
		return (-1); // invalid file descriptor
	}

	// check if it has access to read
	if (!(fcbArray[fd].flags & O_RDWR || fcbArray[fd].flags & O_RDONLY))
	{
		printf("[ERROR] does not have access to read\n");
		return -1;
	}

	int bufferLength = (count/B_CHUNK_SIZE + 2) * B_CHUNK_SIZE; // 1 Extra just incase
	fcbArray[fd].buf = malloc(bufferLength);	
	if(fcbArray[fd].buf == NULL) 
	{ 
		printf("[ERROR]: Not enough main memory.\n");
		return 0;
	}
	fcbArray[fd].buflen = bufferLength;
	
	// if user is asking for more bytes, then update size
	if ((fcbArray[fd].index + count) > fcbArray[fd].dirEntry->reclen)
	{
		// TODO: Allocate more space
	}

	int startBlock = fcbArray[fd].index / sffs_vcb->block_size; 
	int leftover_bytes = count;  // Bytes needed to be read from disk
	int numOfBlocksNeeded = (leftover_bytes / sffs_vcb->block_size) + 1;

	int blocksRead = 0;  // Blocks Read from Disk
	int bytesRead = 0;
	while(leftover_bytes > 0 && blocksRead != 0)
	{
		// Read from Disk
		blocksRead = getBlockXofFile(&fcbArray[fd], startBlock, numOfBlocksNeeded);	
		
		if(blocksRead < numOfBlocksNeeded)
		{
			bytesRead = blocksRead * sffs_vcb->block_size;
		}
		else // Have enough/more than enough blocks
		{
			bytesRead = leftover_bytes;
		}
		memcpy(buffer, fcbArray[fd].buf + fcbArray[fd].index, bytesRead);

		// Update variables
		leftover_bytes -= bytesRead;
		numOfBlocksNeeded -= blocksRead;
		fcbArray[fd].index += bytesRead; 
		startBlock += blocksRead;
	}
	// Part 3 TODO: Initialize/refill the leftover

	return blocksRead;
}

// Interface to Close the file
// @param fd, a file descriptor
void b_close(b_io_fd fd)
{
	int temp_size;
	// Update Directory entry? or was it file descriptor? TODO: Check April 21 lecture recording
	if (fcbArray[fd].index < B_CHUNK_SIZE)
	{
		char *temp_buffer = malloc(B_CHUNK_SIZE);
		memcpy(temp_buffer, fcbArray[fd].buf, fcbArray[fd].index + 1);
		overwriteAllocated(temp_buffer, 1, fcbArray[fd].dirEntry->location + fcbArray[fd].index);
		temp_size = fcbArray[fd].dirEntry->size;
	}
	// Commit Changes
	uint64_t blocksNeeded = (DIR_ENTRY_LIST_SIZE / sffs_vcb->block_size) + 1;
	dir_entry *temp_dir = malloc(blocksNeeded * sffs_vcb->block_size);
	char *last_token = malloc(256);

	bool valid = pathParser(fcbArray[fd].dirEntry->name, EXIST_FILE, temp_dir, last_token);

	if (!valid)
	{
		printf("[ERROR] Invalid path\n");
		free(fcbArray[fd].buf);
		fcbArray[fd].buf = NULL;
		fcbArray[fd].dirEntry = NULL;
		return;
	}
	int dir_entry_index = -1;
	for (int i = 2; i < DE_CAPACITY; i++)
	{
		if (strcmp(temp_dir[i].name, last_token) == 0)
		{
			dir_entry_index = i;
			break;
		}
	}
	if (dir_entry_index == -1)
	{
		printf("[ERROR] cannot find dir_entry_index\n");
		free(fcbArray[fd].buf);
		fcbArray[fd].buf = NULL;
		fcbArray[fd].dirEntry = NULL;
		return;
	}
	temp_dir[dir_entry_index].size = temp_size;
	overwriteAllocated(temp_dir, temp_dir[0].size, temp_dir[0].location);
	// Free FCB
	free(fcbArray[fd].buf);
	fcbArray[fd].buf = NULL;
	fcbArray[fd].dirEntry = NULL;
}
