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
* Description: Volume Control Block
*
**************************************************************/
#include "fsLow.h"

#ifndef VCB_H
#define VCB_H

typedef struct vcb_s {
    // disk signature defined as 4 chars
    char disk_sig[4];

    // uint64_t = 8 bytes

    // vcb information
    uint64_t block_count;       // number of blocks in volume
    uint64_t block_size;        // size in bytes per block
    uint64_t vcb_location;      // where vcb is in volume (block num)

    // Free space
    uint64_t fs_start_location; // where free space starts in volume (block num)
    uint64_t fs_next_location;  // next free block location (block num)

    // Root
    uint64_t root_location;     // where root dir starts in volume (block num)
    uint64_t root_size;         // size of root in volume in blocks

    // FAT
    uint64_t fat_location;      // where FAT is in volume
    uint64_t fat_size;          // size of FAT in volume in blocks

    // 76 bytes

    
} vcb;

#endif

vcb* sffs_vcb;