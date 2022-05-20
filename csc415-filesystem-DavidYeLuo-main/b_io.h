/**************************************************************
* Class:  CSC-415-01 Fall 2021
* Names: Christian Francisco, David Ye Luo, Marc Castro, Rafael Sunico
* Student IDs: 920603057, 917051959, 921720147, 920261261
* GitHub Name: csc415-filesystem-DavidYeLuo
* Group Name: Segmentation Fault
* Project: Basic File System
*
* File: b_io.h
*
* Description: Basic File System - Key File I/O Operations
*
**************************************************************/

#ifndef _B_IO_H
#define _B_IO_H
#include <fcntl.h>

typedef int b_io_fd;

b_io_fd b_open (char * filename, int flags);
int b_read (b_io_fd fd, char * buffer, int count);
int b_write (b_io_fd fd, char * buffer, int count);
int b_seek (b_io_fd fd, off_t offset, int whence);
void b_close (b_io_fd fd);

// Helpers 
// these do not need to be public
// uint64_t getBlockXofFile(b_fcb *fcb, uint64_t relativeFileBlockNum, uint64_t block_count);
// uint64_t putBlockXofFile(b_fcb *fcb, uint64_t relativeFileBlockNum, uint64_t block_count);

#endif

