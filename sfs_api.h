#ifndef _INCLUDE_SFS_API_H_
#define _INCLUDE_SFS_API_H_

#include <stdint.h>

#define MAXFILENAME 60
#define KEITHS_DISK "sfs_disk.disk"
#define BLOCK_SZ 1024   // Block size in bytes
#define NUM_BLOCKS 3100  // Number of blocks of the entire disk
#define NUM_INODES 110   // Number of inodes in the inode table
#define MAX_FILE_SIZE BLOCK_SZ * 12 + (BLOCK_SZ / sizeof(unsigned int) * BLOCK_SZ // Max file size in bytes
#define NUM_INODE_BLOCKS (sizeof(inode_t) * NUM_INODES / BLOCK_SZ + 1) // Number of blocks needed to store the inode table, +1 to give ceiling, rather than floor
#define MAX_DIRECTORY_ENTRIES NUM_INODES - 1  // Maximum number directory entries in the directory table. We can only have as
                                              // many files as we have available inodes - 1, as the first inode is always for
                                              // the root directory
#define ROOT_DIRECTORY_SIZE_IN_BYTES sizeof(directory_entry_t) * MAX_DIRECTORY_ENTRIES
#define ROOT_DIRECTORY_SIZE_IN_BLOCKS ROOT_DIRECTORY_SIZE_IN_BYTES / BLOCK_SZ
#define FD_TABLE_SIZE NUM_INODES - 1
#define NUM_INDIRECT_POINTERS BLOCK_SZ/sizeof(int)
#define NUM_DIRECT_POINTERS = 12

typedef struct {
    uint64_t magic;       // These are unsigned long long ints
    uint64_t block_size;
    uint64_t fs_size;
    uint64_t inode_table_len;
    uint64_t root_dir_inode;
} superblock_t;

typedef struct {
    unsigned int size;      // Size of file, in bytes.
    unsigned int is_used;      // An addition - not normally in an inode but I add it to track whether the inode is used or not. If 1, used, if 0, free.
    unsigned int data_ptrs[NUM_DIRECT_POINTERS]; // Direct pointers
    unsigned int indirect_ptr;  // An indirect ptr. It's value is a the number of a block containing BLOCK_SZ/4 direct pointers
} inode_t;

/*
 * inode    which inode this entry describes
 * rwptr    where in the file to start
 */
typedef struct {
    uint64_t inode_no; // The inode number
    uint64_t rwptr; // The byte of the file the rwpointer is at
} file_descriptor_t;  // The file descriptor's number is the index into the merged file descriptor and open files table

/**
 * Directory entry
 * inode - the inode number for the file
 * file_name - the name of the file
 */
typedef struct {
    uint64_t inode_no;
    char* file_name;
} directory_entry_t;

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
