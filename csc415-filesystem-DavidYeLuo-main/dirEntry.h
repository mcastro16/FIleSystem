/**************************************************************
* Class:  CSC-415-01 Fall 2021
* Names: Christian Francisco, David Ye Luo, Marc Castro, Rafael Sunico
* Student IDs: 920603057, 917051959, 921720147, 920261261
* GitHub Name: csc415-filesystem-DavidYeLuo
* Group Name: Segmentation Fault
* Project: Basic File System
*
* File: dirEntry.h
*
* Description: Basic directory information.
**************************************************************/
#include <stdbool.h>

#include "fsLow.h"

#define NAME_SIZE            64
#define EXT_SIZE             3  // TODO: Remove this

#define DE_CAPACITY          50 // Number of directory entry in a directory (Initial size)

#define NOT_EXIST 0x00000000 // Path is valid, but last token in path does not exist
#define EXIST_FILE 0x00000001 // Path is valid, and last token matches a file
#define EXIST_DIR 0x00000002 // Path is valid, and last token matches a directory
#define EXIST 0x00000004 // Path is valid, and last token matches a directory or file

#define DIR_ENTRY_LIST_SIZE (sizeof(dir_entry) * DE_CAPACITY)

#ifndef DIRENTRY_H
#define DIRENTRY_H
typedef struct dir_entry_s
    {
    char name[NAME_SIZE]; // name of directory
    bool isDir;
    uint64_t location; // block location
    uint64_t size; // number of blocks 

    unsigned short reclen; // number of bytes

    // TODO: Add create, lastMod, lastAccess time
    } dir_entry;    

#endif  

uint64_t createDir(uint64_t parentLocation);
bool pathParser(const char *path, int last_token_flag, dir_entry *temp_dir, char *last_token_in_path);
