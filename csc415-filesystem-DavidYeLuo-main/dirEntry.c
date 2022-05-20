/**************************************************************
 * Class:  CSC-415-01 Fall 2021
 * Names: Christian Francisco, David Ye Luo, Marc Castro, Rafael Sunico
 * Student IDs: 920603057, 917051959, 921720147, 920261261
 * GitHub Name: csc415-filesystem-DavidYeLuo
 * Group Name: Segmentation Fault
 * Project: Basic File System
 *
 * File: dirEntry.c
 *
 * Description: Basic directory management functions.
 **************************************************************/
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#include "dirEntry.h"
#include "fsFree.h"
#include "mfs.h"
#include "vcb.h"

uint64_t createDir(uint64_t parentLocation)
{
    uint64_t blocksNeeded = (DIR_ENTRY_LIST_SIZE / sffs_vcb->block_size) + 1;
    dir_entry *directory = malloc(blocksNeeded * sffs_vcb->block_size);

    // Set to a known state
    for (int i = 0; i < DE_CAPACITY; i++)
    {
        strcpy(directory[i].name, "\0");
        directory[i].isDir = 0; // false
        directory[i].size = 0;
        directory[i].reclen = 0;
        // TODO: Add create, lastMod, lastAccess time
    }
    uint64_t startBlock = allocateFreeSpace(blocksNeeded);

    // Error checking
    if (startBlock == -1)
    {
        printf("[ERROR] allocateFreeSpace failed\n");
        free(directory);
        return -1;
    }
    strcpy(directory[0].name, ".");
    directory[0].isDir = 1;
    directory[0].location = startBlock;
    directory[0].size = blocksNeeded;
    directory[0].reclen = DIR_ENTRY_LIST_SIZE;
    // TODO: Add create, lastMod, lastAccess time

    strcpy(directory[1].name, "..");
    directory[1].isDir = 1;
    if (parentLocation == 0) // If we are creating the root directory
    {
        directory[1].location = directory[0].location;

        cwd_location = startBlock;
    }
    else
    {
        directory[1].location = parentLocation;
    }
    directory[1].size = blocksNeeded;
    directory[1].reclen = DIR_ENTRY_LIST_SIZE;
    // TODO: Add create, lastMod, lastAccess time

    uint64_t blocksWritten = overwriteAllocated(directory, blocksNeeded, startBlock);

    // clean up
    free(directory);

    if (blocksWritten != blocksNeeded)
    {
        printf("[ERROR] overwriteAllocated wrote %ld instead of %ld.\n", blocksWritten, blocksNeeded);
        return -1;
    }
    return startBlock;
}

bool pathParser(const char *path, int last_token_flag, dir_entry *temp_dir, char *last_token_in_path)
{

    bool is_absolute = false;

    if (path[0] == '/')
    {
        is_absolute = true;
        readAllocated(temp_dir, sffs_vcb->root_size, sffs_vcb->root_location); // load root directory

        if (strcmp(path, "/") == 0) // if path is exactly "/"
        {
            strcpy(last_token_in_path, ".");
            return (last_token_flag == EXIST) || (last_token_flag == EXIST_DIR);
        }
    }
    else
    {
        uint64_t blocksNeeded = (DIR_ENTRY_LIST_SIZE / sffs_vcb->block_size) + 1;
        readAllocated(temp_dir, blocksNeeded, cwd_location); // load current directory
    }

    // copy const path into new var for strtok_r
    char *pathcpy = malloc(strlen(path) + 1);
    strcpy(pathcpy, path);

    // tokenise path
    int token_count = 0;
    char **tokens_arr = malloc((strlen(path) / 2) * sizeof(char *));
    char *rest = NULL;
    char *token;
    for (token = strtok_r(pathcpy, "/", &rest); token != NULL; token = strtok_r(NULL, "/", &rest))
    {
        tokens_arr[token_count] = token;
        token_count++;
    }

    // traverse tokens from paths
    bool found = false;
    for (int tok_index = 0; tok_index < token_count - 1; tok_index++)
    {
        for (int dir_entry_index = 0; dir_entry_index < DE_CAPACITY; dir_entry_index++)
        {
            if (strcmp(temp_dir[dir_entry_index].name, tokens_arr[tok_index]) == 0)
            {
                found = true;
                uint64_t blocks_needed = temp_dir[dir_entry_index].size;
                uint64_t blocks_ret = readAllocated(temp_dir, blocks_needed, temp_dir[dir_entry_index].location);
                if (blocks_ret != blocks_needed)
                {
                    printf("[ERROR] dirEntry.c:133, readAllocated failed\n");
                    free(pathcpy);
                    free(token);
                    free(tokens_arr);
                    return 0;
                }
                break;
            }
        }
        if (!found)
        {
            printf("[ERROR] dirEntry.c, path to last token does not exist\n");
            free(pathcpy);
            free(token);
            free(tokens_arr);
            return 0;
        }
        found = false;
    }

    // find last_token_in_path in dir entries
    int last_token_flag_actual = NOT_EXIST;
    if (token_count != 0)
    {
        strcpy(last_token_in_path, tokens_arr[token_count - 1]);
    }
    for (int i = 0; i < DE_CAPACITY; i++)
    {
        if (strcmp(temp_dir[i].name, last_token_in_path) == 0)
        {
            if (temp_dir[i].isDir)
            {
                last_token_flag_actual = EXIST_DIR;
            }
            else
            {
                last_token_flag_actual = EXIST_FILE;
            }
            break;
        }
    }

    free(pathcpy);
    free(token);
    free(tokens_arr);

    // return
    if (last_token_flag == EXIST)
    {
        return (last_token_flag_actual != NOT_EXIST);
    }

    return (last_token_flag == last_token_flag_actual);
}

// @param pathname, pass this into parsePath, with a directory entry, and a buffer for the last token, to check if this path exists
// @param mode, idk what this is exactly, I think its got to do with permissions,
//     only used with 0777 in fsshell.c:299 which should mean all permissions
// @return 0 for success, -1 for error
int fs_mkdir(const char *pathname, mode_t mode)
{

    // printf(" - making directory\n");

    uint64_t blocksNeeded = (DIR_ENTRY_LIST_SIZE / sffs_vcb->block_size) + 1;
    dir_entry *temp_dir = malloc(blocksNeeded * sffs_vcb->block_size);
    char *last_token = malloc(256);

    bool valid = pathParser(pathname, NOT_EXIST, temp_dir, last_token);

    if (!valid)
    {
        printf("[ERROR] dirEntry.c, path does not exist or dir already exists\n");
        return -1;
    }

    // printf(" - found path %s\n", temp_dir[0].name);
    // printf(" - found token %s\n", last_token);

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
        printf("[ERROR] dirEntry.c, directory is full\n");
        free(temp_dir);
        free(last_token);
        return -1;
    }

    uint64_t new_dir_location = createDir(temp_dir[0].location);
    if (new_dir_location == -1)
    {
        printf("[ERROR] dirEntry.c, failed to make new directory\n");
        free(temp_dir);
        free(last_token);
        return -1;
    }

    strncpy(temp_dir[dir_entry_index].name, last_token, NAME_SIZE);
    temp_dir[dir_entry_index].size = blocksNeeded;
    temp_dir[dir_entry_index].reclen = DIR_ENTRY_LIST_SIZE;
    temp_dir[dir_entry_index].location = new_dir_location;
    temp_dir[dir_entry_index].isDir = true;
    // TODO: Add create, lastMod, lastAccess time

    printf(" - new dir %s\n", temp_dir[dir_entry_index].name);

    uint64_t blocks_ret = overwriteAllocated(temp_dir, blocksNeeded, temp_dir[0].location);
    if (blocks_ret != blocksNeeded)
    {
        printf("[ERROR] dirEntry.c:251, overwriteAllocated failed\n");
        free(temp_dir);
        free(last_token);
        return -1;
    }

    free(temp_dir);
    free(last_token);

    return 0;
}

// removes associated directory entry and frees allocated blocks in volume
// @param filename, path to file
// @return 0 for success, -1 for error
int fs_delete(char *filename)
{
    // validate path and load parent directory entry and get last token
    uint64_t blocksNeeded = (DIR_ENTRY_LIST_SIZE / sffs_vcb->block_size) + 1;
    dir_entry *temp_dir = malloc(blocksNeeded * sffs_vcb->block_size);
    char *last_token = malloc(256);

    bool valid = pathParser(filename, EXIST, temp_dir, last_token);

    if (!valid)
    {
        printf("[ERROR] dirEntry.c, path does not exist\n");
        return -1;
    }

    // find directory entry of file/dir for removal
    int dir_entry_index = -1;
    for (int i = 2; i < DE_CAPACITY; i++)
    {
        if (strcmp(temp_dir[i].name, last_token) == 0)
        {
            dir_entry_index = i;
            break;
        }
    }
    
    // free allocated blocks in FAT
    int ret = releaseFreeSpace(temp_dir[dir_entry_index].location);
    if (ret == -1)
    {
        printf("[ERROR] dirEntry.c releaseFreeSpace failed\n");
    }

    // remove directory entry
    strcpy(temp_dir[dir_entry_index].name, "\0");
    temp_dir[dir_entry_index].isDir = 0;
    temp_dir[dir_entry_index].size = 0;
    temp_dir[dir_entry_index].reclen = 0;
    // TODO: Add create, lastMod, lastAccess time

    // overwrite parent directory entry
    uint64_t write_blocks_needed = temp_dir[0].size;
    uint64_t blocks_written = overwriteAllocated(temp_dir, temp_dir[0].size, temp_dir[0].location);
    if (blocks_written != write_blocks_needed)
    {
        printf("[ERROR] dirEntry.c:305 overwriteAllocated failed\n");
        free(temp_dir);
        free(last_token);
        return -1;
    }

    free(temp_dir);
    free(last_token);

    return 1;
}

// removes a directory
// @param pathname, path to dir to delete
// @return 0 for success, -1 for error
int fs_rmdir(const char *pathname)
{
    // validate path, get parent directory and last token
    uint64_t blocksNeeded = (DIR_ENTRY_LIST_SIZE / sffs_vcb->block_size) + 1;
    dir_entry *temp_dir = malloc(blocksNeeded * sffs_vcb->block_size);
    char *last_token = malloc(256);

    bool valid = pathParser(pathname, EXIST, temp_dir, last_token);

    if (!valid)
    {
        printf("[ERROR] dirEntry.c, path does not exist\n");
        free(temp_dir);
        free(last_token);
        return -1;
    }

    // get directory from parent directory
    for (int i = 2; i < DE_CAPACITY; i++)
    {
        if (strcmp(temp_dir[i].name, last_token) == 0)
        {
            uint64_t read_blocks_needed = temp_dir[i].size;
            uint64_t blocks_read = readAllocated(temp_dir, temp_dir[i].size, temp_dir[i].location);
            if (blocks_read != read_blocks_needed) {
                printf("[ERROR] dirEntry.c:343 readAllocated failed\n");
                free(temp_dir);
                free(last_token);
                return -1;
            }
            break;
        }
    }

    // error if root
    if (temp_dir[0].location == sffs_vcb->root_location)
    {
        printf("[ERROR] fs_rmdir attempted to remove root directory\n");
        free(temp_dir);
        free(last_token);
        return -1;
    }

    // error if current working directory
    if (temp_dir[0].location == cwd_location)
    {
        printf("[ERROR] fs_rmdir attempted to remove current directory\n");
        free(temp_dir);
        free(last_token);
        return -1;
    }

    // error if directory still has valid child directory entries
    for (int i = 2; i < DE_CAPACITY; i++)
    {
        if (strcmp(temp_dir[i].name, "\0") != 0)
        {
            printf("[ERROR] fs_rmdir attempted to remove non-empty directory\n");
            free(temp_dir);
            free(last_token);
            return -1;
        }
    }

    // delete from volume
    int ret = fs_delete((char *)pathname);
    if (ret == -1) 
    {
        printf("[ERROR] fs_delete failed\n");
        free(temp_dir);
        free(last_token);
        return -1;
    }

    // overwrite parent directory
    uint64_t write_blocks_needed = temp_dir[0].size;
    uint64_t blocks_written = overwriteAllocated(temp_dir, temp_dir[0].size, temp_dir[0].location);
    if (blocks_written != write_blocks_needed)
    {
        printf("[ERROR] dirEntry.c:375 overwriteAllocated failed\n");
        free(temp_dir);
        free(last_token);
        return -1;
    }

    free(temp_dir);
    free(last_token);

    return 1;
}

// @param name, name of directory to open (relative path)
// @return fdDir, Directory Descriptor
fdDir *fs_opendir(const char *name)
{
    uint64_t blocksNeeded = (DIR_ENTRY_LIST_SIZE / sffs_vcb->block_size) + 1;
    dir_entry *temp_dir = malloc(blocksNeeded * sffs_vcb->block_size);
    char *last_token = malloc(256);

    bool valid = pathParser(name, EXIST_DIR, temp_dir, last_token);

    if (!valid)
    {
        printf("[ERROR] dirEntry.c, dir does not exist\n");
        free(temp_dir);
        free(last_token);
        return NULL;
    }

    int dir_entry_index;
    for (int i = 0; i < DE_CAPACITY; i++)
    {
        if (strcmp(temp_dir[i].name, last_token) == 0)
        {
            dir_entry_index = i;
            break;
        }
    }

    // if (strcmp(last_token, ".") == 0)
    //{
    //     dir_entry_index = 0;
    // }

    fdDir *dirp = malloc(sizeof(dirp));

    dirp->d_reclen = temp_dir[dir_entry_index].reclen;
    dirp->dirEntryPosition = 0;
    dirp->directoryStartLocation = temp_dir[dir_entry_index].location;
    strcpy(dirp->name, temp_dir[dir_entry_index].name);
    if (temp_dir[dir_entry_index].isDir)
    {
        dirp->fileType = DT_DIR;
    }
    else
    {
        dirp->fileType = DT_REG;
    }

    // TODO: Add create, lastMod, lastAccess time

    free(temp_dir);
    free(last_token);

    return dirp;
}

// @param dirp, Directory Descriptor
// @returns fs_diriteminfo, List of File Descriptor
struct fs_diriteminfo *fs_readdir(fdDir *dirp)
{

    // Validate dirp and size
    //                              do we need this?
    // int infoSize = 512;
    // if (NAME_SIZE > infoSize) // (Validate for strcpy)
    //{
    //    printf("ERROR: dirEntry NAME_SIZE exceeds fdDir size. ");
    //    printf("Should set NAME_SIZE smaller. ");
    //    printf("Returning NULL\n");
    //    return NULL;
    //}
    if (dirp == NULL)
    {
        printf("ERROR: param *dirp is NULL.\n");
        printf("Returning NULL.\n");
        return NULL;
    }

    // Init dirInfo
    struct fs_diriteminfo *dirInfo = malloc(sizeof(struct fs_diriteminfo));

    // Prepare for readAllocated
    uint64_t blocksNeeded = (DIR_ENTRY_LIST_SIZE / sffs_vcb->block_size) + 1;
    dir_entry *temp_dir = malloc(blocksNeeded * sffs_vcb->block_size);

    // Update/Assign entry position to each directory entries
    readAllocated(temp_dir, blocksNeeded, dirp->directoryStartLocation);

    bool isDirNameEmpty = strcmp(temp_dir[dirp->dirEntryPosition].name, "\0") == 0;

    while (isDirNameEmpty && dirp->dirEntryPosition < DE_CAPACITY - 1)
    {
        dirp->dirEntryPosition++;

        isDirNameEmpty = strcmp(temp_dir[dirp->dirEntryPosition].name, "\0") == 0;
    }

    if (dirp->dirEntryPosition == DE_CAPACITY - 1)
    {
        // printf("[DEBUG] FS_READDIR: Have reached end of directory. ");
        // printf("Exitting.\n");
        free(temp_dir);
        return NULL;
    }

    // Copy dirp to dirInfo
    strcpy(dirInfo->d_name, temp_dir[dirp->dirEntryPosition].name);
    dirInfo->d_reclen = dirp->d_reclen;
    dirInfo->fileType = dirp->fileType;

    // TODO: Add create, lastMod, lastAccess time

    // Update entry position
    dirp->dirEntryPosition++;

    free(temp_dir);

    return dirInfo;
}

// @param path, path to directory
// @param buf, fs_stat struct to fill in in this func?, check mfs.h:88
// @return 0 for success, -1 for error
int fs_stat(const char *path, struct fs_stat *buf)
{
    uint64_t blocksNeeded = (DIR_ENTRY_LIST_SIZE / sffs_vcb->block_size) + 1;
    dir_entry *temp_dir = malloc(blocksNeeded * sffs_vcb->block_size);
    char *last_token_in_path = malloc(256);

    bool valid = pathParser(path, EXIST, temp_dir, last_token_in_path);

    int dir_entry_index;
    if (valid)
    {
        for (int i = 0; i < DE_CAPACITY; i++)
        {
            if (strcmp(last_token_in_path, temp_dir[i].name) == 0)
            {
                dir_entry_index = i;
                break;
            }
        }
    }
    else
    {
        printf("Error: invalid path\n");
        free(temp_dir);
        free(last_token_in_path);
        return -1;
    }

    buf->st_size = (off_t)temp_dir[dir_entry_index].reclen;
    buf->st_blksize = (blksize_t)sffs_vcb->block_size;
    buf->st_blocks = (blkcnt_t)blocksNeeded;
    time_t temp_time_val;
    time(&temp_time_val);
    buf->st_accesstime = temp_time_val;
    buf->st_createtime = temp_time_val;
    buf->st_modtime = temp_time_val;

    free(temp_dir);
    temp_dir = NULL;
    free(last_token_in_path);
    last_token_in_path = NULL;

    return 0;
}

int fs_closedir(fdDir *dirp)
{
    if (dirp == NULL)
    {
        printf("[ERROR] fsDir.c closedir: invalid argument\n");
        return -1;
    }

    free(dirp);
    dirp = NULL;

    return 0;
}

// @param buf, char buffer to put path name into
// @param size, max bytes to put into buf
// @return char*, pointer to path name
char *fs_getcwd(char *buf, size_t size)
{
    // printf("DEBUG: Inside getcwd\n");
    char **token_array = malloc(size);
    uint64_t blocks_needed = (DIR_ENTRY_LIST_SIZE / sffs_vcb->block_size) + 1;
    dir_entry *temp_directory = malloc(blocks_needed * sffs_vcb->block_size);

    printf("cwd: %ld\n", cwd_location);

    readAllocated(temp_directory, blocks_needed, cwd_location);
    int find_name = temp_directory[0].location;
    int token_count = 0;

    while (temp_directory[1].location != temp_directory[0].location)
    {
        readAllocated(temp_directory, blocks_needed, temp_directory[1].location);
        for (int i = 2; i < DE_CAPACITY; i++)
        {
            if (temp_directory[i].location == find_name)
            {
                token_array[token_count] = malloc(sizeof(token_array) * strlen(temp_directory[i].name) + 1);
                strcpy(token_array[token_count], temp_directory[i].name);
                token_count++;
                break;
            }
        }
        find_name = temp_directory[0].location;
    }
    char *path = malloc(size + token_count - 1);
    strcpy(path, "/");
    for (int i = token_count - 1; i >= 0; i--)
    {
        strcat(path, token_array[i]);
        if (i > 0)
        {
            strcat(path, "/");
        }
    }
    strcpy(buf, path);

    free(temp_directory);
    temp_directory = NULL;
    free(token_array);
    token_array = NULL;

    return path;
}

// @param buf, path to directory
// @return 0 for success, -1 for error
int fs_setcwd(char *buf)
{
    // printf("DEBUG: Inside setcwd\n");
    uint64_t blocksNeeded = (DIR_ENTRY_LIST_SIZE / sffs_vcb->block_size) + 1;
    dir_entry *temp_directory = malloc(blocksNeeded * sffs_vcb->block_size);
    char *final_token = malloc(256);

    bool check_path = pathParser(buf, EXIST_DIR, temp_directory, final_token);

    if (check_path)
    {
        for (int i = 0; i < DE_CAPACITY; i++)
        {
            if (strcmp(temp_directory[i].name, final_token) == 0)
            {
                cwd_location = temp_directory[i].location;
                break;
            }
        }
        free(temp_directory);
        temp_directory = NULL;
        free(final_token);
        final_token = NULL;
        return 0;
    }
    else
    {
        printf("ERROR: invalid path\n");
        free(temp_directory);
        temp_directory = NULL;
        free(final_token);
        final_token = NULL;
        return -1;
    }
}

// @param path
// @return 1 if file, 0 otherwise
int fs_isFile(char *path)
{
    uint64_t blocksNeeded = (DIR_ENTRY_LIST_SIZE / sffs_vcb->block_size) + 1;
    dir_entry *temp_dir = malloc(blocksNeeded * sffs_vcb->block_size);
    char *last_token = malloc(256);

    bool valid = pathParser(path, EXIST_FILE, temp_dir, last_token);

    free(temp_dir);
    free(last_token);

    return valid;
}

// @param path
// @return 1 if directory, 0 otherwise
int fs_isDir(char *path)
{
    uint64_t blocksNeeded = (DIR_ENTRY_LIST_SIZE / sffs_vcb->block_size) + 1;
    dir_entry *temp_dir = malloc(blocksNeeded * sffs_vcb->block_size);
    char *last_token = malloc(256);

    bool valid = pathParser(path, EXIST_DIR, temp_dir, last_token);

    free(temp_dir);
    free(last_token);

    return valid;
}
