#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>    // for `ffs`
#include "sfs_api.h"
#include "disk_emu.h"

// In-memory cached data structures
superblock_t sb;
inode_t inode_table[NUM_INODES]; // The inode table, an array of inode structs
int inode_table_status[NUM_INODES] = {[0 ... NUM_INODES-1] = 1}; // Remembers which inodes of the inode table are used (0) and which are free (1)
directory_entry directory_table[MAX_DIRECTORY_ENTRIES];  // The directory table, an array of directory entry structs
uint8_t free_bit_map[BIT_MAP_SIZE] = { [0 ... BIT_MAP_SIZE-1] = UINT8_MAX };

file_descriptor fdt[NUM_INODES];

void init_superblock() {
    sb.magic = 0xACBD0005;
    sb.block_size = BLOCK_SZ;
    sb.fs_size = NUM_BLOCKS * BLOCK_SZ;
    sb.inode_table_len = NUM_INODE_BLOCKS;
    sb.root_dir_inode = 0; // The first inode in the inode table is for the root directory
}

void mksfs(int fresh) {
	  //Implement mksfs here
    if (fresh) {
        printf("making new file system\n");
        // create super block
        init_superblock();

        init_fresh_disk(KEITHS_DISK, BLOCK_SZ, NUM_BLOCKS);

        /* write super block
         * write to first block, and only take up one block of space
         * pass in sb as a pointer
         */
        write_blocks(0, 1, &sb);

        // write inode table
        write_blocks(1, sb.inode_table_len, inode_table);

        // write free block list
        write_blocks(1+sb.inode_table_len, NUM_BIT_MAP_BLOCKS,  free_bit_map);

    } else {
        printf("reopening file system\n");
        // initialize the disk
        init_disk(KEITHS_DISK, BLOCK_SZ, NUM_BLOCKS);

        // read the super block from disk into memory
        read_blocks(0, 1, &sb);
        printf("Block Size is: %llu\n", sb.block_size);

        // read the inode table from disk into memory
        read_blocks(1, sb.inode_table_len, inode_table);

        // read the free bit map from disk into memory
        read_blocks(1+sb.inode_table_len, NUM_BIT_MAP_BLOCKS, free_bit_map);
    }
	  return;
}

/**
 * Copies the name of the next file in the directory into fname
 * Returns 1 if there is another file in the directory, after
 *   the one whose name it just copied into fname
 * Returns 0 if it got the name of the last file int the directory
 */
int sfs_getnextfilename(char *fname) {

	  //Implement sfs_getnextfilename here
    // must remember the current position in the directory at each call
	  return 0;
}


/**
 * Returns the size of a given file, in bytes
 */
int sfs_getfilesize(const char* path) {

	  //Implement sfs_getfilesize here
	  return 0;
}

/**
 * Opens a file and returns the index that corresponds to the newly opened file
 * in the file descriptor table
 * If the file does not exist, it creates a new file with the given name and sets
 * its size to zero
 * If it exists, the file is opened in append mode (the rwpointer is set to the
 * end of the file)
 */
int sfs_fopen(char *name) {

	  //Implement sfs_fopen here

    // Search the directory for the file
    // If file exists, then open it in append mode
    // else,
    // 1. allocate and initialize an inode. Figure out an empty inode slot in the inode table
    // and save the inode to this slot.
    // 2. Write the mapping between the inode and the file name in the root directory: write it to the copy in memory
    // and then write the copy back to disk
    // 3. Set the file size to zero


    /*
     * For now, we only return 1
     */
    //inode_t newInode = malloc(sizeof(inode_t));
    //newInode.size = 0;
    uint64_t test_file = 0; // The file descriptor

    fdt[test_file].inode = 1;
    fdt[test_file].rwptr = 0;

    return test_file;
}

/**
 * Closes a file. All this involves is removing the entry at index fileID
 * from the file descriptor table
 * Returns 0 on success and a negative number otherwise
 */
int sfs_fclose(int fileID){
    //Implement sfs_fclose here
    fdt[0].inode = 0;
    fdt[0].rwptr = 0;
    return 0;
}

/**
 * Read length bytes into buf of the file corresponding to file descriptor
 * table entry fileID, starting with the byte of the file indicated by the file's
 * rwpointer
 * Returns the number of bytes read
 */
int sfs_fread(int fileID, char *buf, int length){
    //Implement sfs_fread here
    file_descriptor* f = &fdt[fileID];
    inode_t* n = &inode_table[f->inode];

    int block = n->data_ptrs[0];
    read_blocks(block, 1, (void*) buf);
    return 0;
}

/**
 * Writes length bytes of buf into the file at index fileID of
 * the file descriptor table, starting at the byte of the current rwpointer
 * as determined from the file descriptor table.
 * This could increase the size of the file, so be wary of this. Need to compare
 * rwpointer + length to size of file as stored int the file's inode entry, which
 * we already have in memory, so it's very simple to get this value
 * Returns the number of bytes written
 */
int sfs_fwrite(int fileID, const char *buf, int length){

	  //Implement sfs_fwrite here

    // Determine if the write will increase the size of the file
    // If so,
    // 1. Allocate disk blocks (mark them as allocated in free block map)
    // 2. Modify file's inode to point to these blocks
    // 3. Write the data in buf to these blocks, which are in mem
    // 4. Flush all the data to disk
    file_descriptor* f = &fdt[fileID];
    inode_t* n = &inode_table[fileID];

    /*
     * We know block 1 is free because this is a canned example
     * You should call a helper function to find a free block
     */

    int block = 20;
    n->data_ptrs[0] = block;
    n->size += length;
    f->rwptr += length;
    write_blocks(block, 1, (void*) buf);

    return 0;
}

/**
 * Move the rwpointer for the file corresponding to fd entry fileID
 * to loc. i.e. change the rwptr property of the file_descriptor struct
 * at index fileID of the file descriptor table to loc.
 * Returns 0 if success and -1 if error (i.e. trying to move the rwpointer
 * outside of the bounds of the file)
 */
int sfs_fseek(int fileID, int loc){

    // TODO
    /* Perform error checking:
     * If loc > size, we are moving the pointer past the end of the file. This
     * is not allowed.
     * If loc is negative, this, of course, is not allowed
     */
    fdt[fileID].rwptr = loc;
	  return 0;
}

/**
 * Removes the file with the given name from the directory entry,
 * releases the file allocation table entries, and releases the data
 * blocks used by the file
 */
int sfs_remove(char *file) {

	//Implement sfs_remove here
	return 0;
}


// MARK -  Bitmap helpers
/**
 * Sets a specific bit as used
 * index is the number of the bit that we wish to set as used
 */
void force_set_index(uint32_t index) {
    int i = index / 8; // i is the index of the entry of the free_bit_map we wish to change
    int which_bit = index % 8;
    USE_BIT(free_bit_map[i], which_bit);
}

/**
 * Gets the number of the first available free bit in the bitmap
 */
uint32_t get_index() {
    uint32_t i = 0;

    // find the first section with a free bit
    // let's ignore overflow for now...
    while (free_bit_map[i] == 0) { i++; }

    // now, find the first free bit
    // ffs has the lsb as 1, not 0. So we need to subtract
    uint8_t bit = ffs(free_bit_map[i]) - 1;

    // set the bit to used
    USE_BIT(free_bit_map[i], bit);

    //return which bit we used
    return i*8 + bit;
}

/**
 * Frees the bit with number "index"
 */
void rm_index(uint32_t index) {

    // get index in array of which bit to free
    uint32_t i = index / 8;

    // get which bit to free
    uint8_t bit = index % 8;

    // free bit
    FREE_BIT(free_bit_map[i], bit);
}
