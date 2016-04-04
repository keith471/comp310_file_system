#ifndef _INCLUDE_SFS_API_H_
#define _INCLUDE_SFS_API_H_

#include <stdint.h>

typedef struct {
    uint64_t magic;       // These are unsigned long long ints
    uint64_t block_size;
    uint64_t fs_size;
    uint64_t inode_table_len;
    uint64_t root_dir_inode;
} superblock_t;

typedef struct {
    unsigned int mode;      // The permissions of the file/folder. Set them but don't worry about them otherwise for this assignment.
    unsigned int link_cnt;  // Number of other programs that have links to the file/folder. Set but don't worry about.
    unsigned int uid;       // ID of owner of file. Set but don't worry about.
    unsigned int gid;       // Group ID of file. Set but don't worry about.
    unsigned int size;      // Size of file, in bytes.
    unsigned int data_ptrs[12]; // Direct pointers
    // TODO indirect pointer
} inode_t;

/*
 * inode    which inode this entry describes
 * rwptr    where in the file to start
 */
typedef struct {
    uint64_t inode; // The inode number
    uint64_t rwptr; // The byte of the file the rwpointer is at
} file_descriptor;  // The file descriptor's number is the index into the merged file descriptor and open files table

/**
 * Directory entry
 * inode - the inode number for the file
 * file_name - the name of the file
 */
typedef struct {
    uint64_t inode;
    char* file_name;
} directory_entry;

void mksfs(int fresh);
int sfs_getnextfilename(char *fname);
int sfs_getfilesize(const char* path);
int sfs_fopen(char *name);
int sfs_fclose(int fileID);
int sfs_fread(int fileID, char *buf, int length);
int sfs_fwrite(int fileID, const char *buf, int length);
int sfs_fseek(int fileID, int loc);
int sfs_remove(char *file);

#endif //_INCLUDE_SFS_API_H_
