/**************************************************************
 * Class:  CSC-415-01 Fall 2021
 * Names: Christian Francisco, David Ye Luo, Marc Castro, Rafael Sunico
 * Student IDs: 920603057, 917051959, 921720147, 920261261
 * GitHub Name: csc415-filesystem-DavidYeLuo
 * Group Name: Segmentation Fault
 * Project: Basic File System
 *
 * File: fsFree.c
 *
 * Description: Free Space Management.
 **************************************************************/
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#include "fsFree.h"
#include "fsLow.h"
#include "vcb.h"

uint32_t *fat_array;
vcb *sffs_vcb;

// init free space
// takes vcb as argument
// sets vcb->fs_next_location
// loads into memory as uint32_t array
// returns location of fat linked list

uint64_t initFreeSpace(vcb *my_vcb) // takes vcb as argument
{
    sffs_vcb = my_vcb;
    printf("debug: initializing fat region\n");

    int uint32_size = sizeof(uint32_t); // 4

    sffs_vcb->fat_size = ((sffs_vcb->block_count * uint32_size) / sffs_vcb->block_size) + 1;

    printf("fat_size: %ld\n", sffs_vcb->fat_size);

    uint64_t fat_location = 1;
    printf("fat_location: %ld\n", fat_location);

    printf("debug: initializing free space\n");

    fat_array = malloc(sffs_vcb->block_count * uint32_size);
    // initialise
    for (uint64_t i = 0; i < sffs_vcb->fat_size + 1; i++)
    {
        fat_array[i] = 0xFFFFFFFE; // set blocks 1 to (fat_size + 1) to reserved
    }
    for (uint64_t i = sffs_vcb->fat_size + 1; i < sffs_vcb->block_count; i++)
    {
        fat_array[i] = 0; // set blocks (fat_size + 2) to free
    }

    sffs_vcb->fs_next_location = sffs_vcb->fat_size + 1;

    // output fat_array
    // for (int i = 0; i < sffs_vcb->block_count; i++) {
    //    if (i % 20 == 0) {
    //        printf("\n");
    //    }
    //    printf("%d, ", fat_array[i]);
    //}
    // printf("\n");

    // save fat_array into volume with LBAwrite

    if (LBAwrite(fat_array, sffs_vcb->fat_size, fat_location) != sffs_vcb->fat_size)
    {
        return -1; // failed to write to disk
    }

    return fat_location; // return fat_location
}

int reloadFreeSpace(vcb *my_vcb)
{ // assuming volume is already formatted, get fat_array from volume with LBAread
    sffs_vcb = my_vcb;
    // return 0 for success, 1 for error
    int uint32_size = sizeof(uint32_t); // 4
    fat_array = malloc(sffs_vcb->fat_size * sffs_vcb->block_size);
    int LBA_result = LBAread(fat_array, sffs_vcb->fat_size, 1);
    if (LBA_result == sffs_vcb->fat_size)
    {
        return 0;
    }
    return 1;
}

uint64_t allocateFreeSpace(uint64_t block_count) // allocates blocks for saving
{
    // takes block_count as arg

    // iterates over fat_array starting at vcb->fs_next_location, to vcb->fs_next_location + block_count
    // and sets those values to allocated, by setting their values to the block number of the next block allocated
    // and finally sets the last block to 0xFFFFFFFF, which is end of file

    // returns location of first block allocated, or -1 if failed
    if (sffs_vcb->fs_next_location + block_count > sffs_vcb->block_size)
    {
        printf("[ERROR] Not enough space available");
        return -1;
    }

    uint64_t start = sffs_vcb->fs_next_location; // start location
    uint64_t last = start;                       // last block location allocated
    uint64_t blocks_allocated = 0;               // total blocks allocated
    uint64_t i = start + 1;                      // iterator

    // iterate over fat_array, allocate blocks that are free, until total blocks allocated == requested block_count
    while (blocks_allocated != block_count - 1)
    {
        if (i == sffs_vcb->block_count) // reached end of fat_array
        {
            printf("[ERROR] Not enough space available");
            return -1;
        }

        if (fat_array[i] == 0) // if block location is free
        {
            fat_array[last] = i; // point last fat entry to this block location
            last = i;
            blocks_allocated++;
        }
        i++;
    }

    // for (int i = start; i < start + block_count; i++) {
    //     fat_array[i] = i + 1;
    // }

    fat_array[last] = 0xFFFFFFFF; // end of file
    blocks_allocated++;

    while (fat_array[i] != 0) // traverse fat until free block is found
    {
        i++;
    }

    sffs_vcb->fs_next_location = i;

    LBAwrite(fat_array, sffs_vcb->fat_size, sffs_vcb->fat_location);
    return start;
}

uint64_t overwriteAllocated(void *buffer, uint64_t block_count, uint64_t block_location)
{
    void *temp_buf = malloc(sffs_vcb->block_size); // temporary buffer to split buffer into block sized chunks

    uint64_t blocks_written = 0; // total number of blocks written
    uint64_t block = block_location;

    // traverse fat_array, and incrementally write buffer into blocks, until total number of blocks written == block_count
    while (blocks_written != block_count)
    {
        memcpy(temp_buf, buffer + (blocks_written * sffs_vcb->block_size), sffs_vcb->block_size);
        uint64_t ret = LBAwrite(temp_buf, 1, block);
        if (ret != 1)
        {
            printf("overwriteAllocated failed to complete write with LBAwrite\n");
            free(temp_buf);
            return blocks_written;
        }
        blocks_written++;

        if (block == 0xFFFFFFFF) // at end of file
        {
            free(temp_buf);
            return blocks_written;
        }

        block = fat_array[block_location];
        if (block == 0xFFFFFFFE) // at reserved block
        {
            printf("overwriteAllocated attempted to overwrite reserved block\n");
            free(temp_buf);
            return blocks_written;
        }
    }

    free(temp_buf);
    return blocks_written;
}

uint64_t readAllocated(void *buffer, uint64_t block_count, uint64_t block_location)
{
    void *temp_buf = malloc(sffs_vcb->block_size); // temporary buffer to incrementally add to buffer

    uint64_t blocks_read = 0; // total number of blocks read
    uint64_t block = block_location;

    // traverse fat_array, and incrementally read buffer into block sized buffers and append to buffer, until total number of blocks read == block_count
    while (blocks_read != block_count)
    {
        uint64_t ret = LBAread(temp_buf, 1, block);
        if (ret != 1)
        {
            printf("readAllocated failed to complete read with LBAread\n");
            free(temp_buf);
            return blocks_read;
        }
        memcpy(buffer + (blocks_read * sffs_vcb->block_size), temp_buf, sffs_vcb->block_size);
        blocks_read++;

        if (block == 0xFFFFFFFF) // at end of file
        {
            free(temp_buf);
            return blocks_read;
        }

        block = fat_array[block_location];
        if (block == 0xFFFFFFFE) // at reserved block
        {
            printf("overwriteAllocated attempted to overwrite reserved block\n");
            free(temp_buf);
            return blocks_read;
        }
    }

    free(temp_buf);
    return blocks_read;
}

int releaseFreeSpace(uint64_t block_location) 
{
    uint64_t block = block_location;
    sffs_vcb->fs_next_location = block_location;

    while (1)
    {
        uint64_t next_block = fat_array[block];
        if (next_block == 0xFFFFFFFE) {
            printf("releaseFreeSpace attempted to overwrite reserved block\n");
            return -1;
        }
        fat_array[block] = 0;
        if (next_block == 0xFFFFFFFF) break;
        block = next_block;
    }

    LBAwrite(fat_array, sffs_vcb->fat_size, sffs_vcb->fat_location);
    return 0;
}
