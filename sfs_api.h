#ifndef _INCLUDE_SFS_API_H_
#define _INCLUDE_SFS_API_H_

#include <stdint.h>

#define MAXFILENAME 60
#define KEITHS_DISK "sfs_disk.disk"
#define BLOCK_SZ 1024   // Block size in bytes
#define NUM_BLOCKS 100  // Number of blocks of the entire disk
#define NUM_INODES 10   // Number of inodes in the inode table
#define MAX_FILE_SIZE BLOCK_SZ * 12 + (BLOCK_SZ / sizeof(unsigned int) * BLOCK_SZ // Max file size in bytes
#define NUM_INODE_BLOCKS (sizeof(inode_t) * NUM_INODES / BLOCK_SZ + 1) // Number of blocks needed to store the inode table, +1 to give ceiling, rather than floor
#define NUM_BIT_MAP_BLOCKS (NUM_BLOCKS / (8 * BLOCK_SZ) + 1) // Number of blocks needed to store the bitmap; +1 to give ceiling
#define MAX_DIRECTORY_ENTRIES NUM_INODES - 1  // Maximum number directory entries in the directory table. We can only have as
                                              // many files as we have available inodes - 1, as the first inode is always for
                                              // the root directory

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

// MARK - bitmap stuff
/**
 * NUM_BLOCKS is the total number of blocks on disk
 * Thus, SIZE is the total number of entries we need in our bitmap array, since
 * each entry contains an unsigned char of 8 bits
 */
#define BIT_MAP_SIZE (NUM_BLOCKS/8)

/* macros */
#define FREE_BIT(_data, _which_bit) \
    _data = _data | (1 << _which_bit)

#define USE_BIT(_data, _which_bit) \
    _data = _data & ~(1 << _which_bit)

/*
 * @short force an index to be set.
 * @long Use this to setup your superblock, inode table and free bit map
 *       This has been left unimplemented. You should fill it out.
 *
 * @param index index to set
 *
 */
void force_set_index(uint32_t index);

/*
 * @short find the first free data block
 * @return index of data block to use
 */
uint32_t get_index();

/*
 * @short frees an index
 * @param index the index to free
 */
void rm_index(uint32_t index);

#endif //_INCLUDE_SFS_API_H_
