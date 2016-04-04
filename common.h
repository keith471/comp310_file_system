// Just one place for defining a bunch of constants, which may or may not be used all over
#ifndef _INCLUDE_COMMON_H_
#define _INCLUDE_COMMON_H_

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

#endif //_INCLUDE_COMMON_H_
