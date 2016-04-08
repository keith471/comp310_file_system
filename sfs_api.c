#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>    // for `ffs`
#include "sfs_api.h"
#include "disk_emu.h"

#define NUM_BIT_MAP_BLOCKS (sizeof(free_bit_map) / BLOCK_SZ + 1)  // Number of blocks needed to store the bitmap
// In-memory cached data structures

// The super block
superblock_t sb;

// The inode table, an array of inode structs
inode_t inode_table[NUM_INODES];

// The directory table, an array of directory entry structs
directory_entry_t directory_table[MAX_DIRECTORY_ENTRIES];

// The free bit map; 1 bit per block
uint8_t free_bit_map[BIT_MAP_SIZE] = { [0 ... BIT_MAP_SIZE-1] = UINT8_MAX };

// The file descriptor table. Keeps track of the files that are currently open
// We can have a maximum of NUM_INODES - 1 files open at once, since we have NUM_INODES - 1 inodes available for files
file_descriptor_t fd_table[FD_TABLE_SIZE];

//#define NUM_BIT_MAP_BLOCKS (NUM_BLOCKS / (8 * BLOCK_SZ) + 1) // Number of blocks needed to store the bitmap; +1 to give ceiling

// The file descriptor table
file_descriptor_t fdt[NUM_INODES];


/***********************************
 * A boat-load of helper functions
 ***********************************/

void init_superblock() {
    sb.magic = 0xACBD0005;
    sb.block_size = BLOCK_SZ;
    sb.fs_size = NUM_BLOCKS * BLOCK_SZ;
    sb.inode_table_len = NUM_INODE_BLOCKS;
    sb.root_dir_inode = 0; // The first inode in the inode table is for the root directory
}

/**
 * Initializes the first inode entry in the inode table with the information for the root directory
 */
void init_root_dir_inode() {

    inode_table[0].size = ROOT_DIRECTORY_SIZE_IN_BYTES;
    inode_table[0].is_used = 0;

    int indirect_ptrs[NUM_INDIRECT_POINTERS];
    // Determine the blocks that will store the root directory table
    for(int i = 0; i < ROOT_DIRECTORY_SIZE_IN_BLOCKS; i++) {
        int index = get_index(); // Get next free index in the bitmap
        if (i < NUM_DIRECT_POINTERS) {
            inode_table[0].data_ptrs[i] = index;
        } else if (i == NUM_DIRECT_POINTERS) {
            // set the indirect ptr to point to a block
            inode_table[0].indirect_ptr = index;
            // Get another block number for the 12th block of the directory table
            index = get_index();
            indirect_ptrs[0] = index;
        } else {
            indirect_ptrs[i - NUM_DIRECT_POINTERS] = index;
        }
    }

    // Write the block of indirect pointers to disk, if necessary
    if (inode_table[0].indirect_ptr != 0) {
        write_blocks(inode_table[0].indirect_ptr, 1, indirect_ptrs);
    }

}

/**
 * Takes a file name and returns the index of the directory table entry for the file,
 * or -1 if the file was not found
 */
int get_directory_index_for_file_with_name(char *file_name) {
    for (int i = 0; i < MAX_DIRECTORY_ENTRIES; i++) {
        if (strcmp(directory_table[i].file_name, file_name) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * Iterates through the file descriptor table in search of the given inode number. Returns the index
 * containing the inode number or -1 if the inode number is not in the table
 */
int get_fd_for_file_with_inode(int inode_no) {
    for (int i = 0; i < NUM_INODES; i++) {
        if (fd_table[i].inode_no == inode_no) {
            return i;
        }
    }
    return -1;
}

/**
 * Returns the next available file descriptor by iterating through the fd_table
 * and returning the first index with inode_no = 0 (this indicates an unused entry)
 * Returns -1 if no available file descriptors left
 */
int get_next_available_fd() {
    for (int i = 0; i < FD_TABLE_SIZE; i++) {
        if (fd_table[i].inode_no == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * Returns the number corresponding to earliest free inode in the inode table,
 * or -1 if all inodes are used
 */
int get_next_available_inode() {
    for (int i = 0; i < NUM_INODES; i++) {
        if (!inode_table[i].is_used) {
            return i;
        }
    }
    return -1;
}

/**
 * Iterates through the directory and returns the index of the first available entry (where inode_no == 0),
 * or -1 if no available entries
 */
int get_next_available_directory_entry() {
    for (int i = 0; i < MAX_DIRECTORY_ENTRIES; i++) {
        if (directory_table[i].inode_no == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * Sets the properties for the inode at index inode_no of the inode_table, and flushes the inode table to disk
 */
void initialize_new_inode(int inode_no) {
    inode_table[inode_no].size = 0;
    inode_table[inode_no].is_used = 1;
    flush_inode_table();
}

/**
 * Adds a file to the fd table. Returns the fd for the file if successful and -1 if error
 */
int add_to_fd_table(int inode_no, int rwptr) {
    int fd = get_next_available_fd();
    if (fd != -1) {
        fd_table[fd].inode_no = inode_no;
        fd_table[fd].rwptr = rwptr;
        return fd;
    } else {
        printf("Error: You cannot open any more files! No more file descriptors are available.\n");
        return -1;
    }
}

/**
 * Adds the file name and inode number to the root directory, and returns the index of the root directory
 * at which the entry was inserted, or -1 if no available entries remaining
 * Also, flushes the root directory to disk
 */
int add_to_root_directory(int inode_no, char* file_name) {
    int insert_index = get_next_available_directory_entry();
    if (insert_index != -1) {
        directory_table[insert_index].inode_no = inode_no;
        strcpy(directory_table[insert_index].file_name, file_name);
        flush_root_directory();
        return insert_index;
    } else {
        printf("Error: Cannot add entry to root directory as the directory contains no more free space!");
        return -1;
    }
}

/**
 * Returns the block number that contains the byte_no'th byte of the file with inode inode_no
 * inode_no = the inode corresponding to the file or directory
 * byte_no = the number of a byte that resides in the block we want to find
 */
int get_block_number_containing_byte_for_inode(int inode_no, int byte_no) {
    inode_t inode = inode_table[inode_no];
    // Error checking
    if (inode.size > byte_no + 1) {
        printf("Error: Attempting to access a byte beyond the scope of the file.\n");
        return -1;
    }

    // We can take the byte_no and determine whether we want the 1st, 2nd, 3rd, 4th, ..., nth block of the file
    // i.e. we can determine which pointer we want
    int ptr_no = byte_no / BLOCK_SZ;

    if (ptr_no < NUM_DIRECT_POINTERS) {
        return inode.data_ptrs[ptr_no];
    } else {
        // We need to read the block of number inode.indirect_ptr into memory
        int indirect_ptrs[NUM_INDIRECT_POINTERS];
        read_blocks(inode.indirect_ptr, 1, indirect_ptrs);
        // Now, we need to return the (ptr_no - NUM_DIRECT_POINTERS)th pointer in the indirect_ptrs array
        return indirect_ptrs[ptr_no - NUM_DIRECT_POINTERS];
    }
}

/****************
 * Flush helpers
 */

/**
 * Flush everything to disk
 */
void flush_all() {
    flush_superblock();
    flush_free_bit_map();
    flush_inode_table();
    flush_root_directory();
}

/**
 * Flush superblock
 */
void flush_superblock() {
    write_blocks(0, 1, &sb);
}

 /**
  * Flush free bit map
  */
void flush_free_bit_map() {
    write_blocks(1, NUM_BIT_MAP_BLOCKS,  free_bit_map);
}

/**
 * Flush inode table
 */
void flush_inode_table() {
    write_blocks(1 + NUM_BIT_MAP_BLOCKS, sb.inode_table_len, inode_table);
}

// QUESTION
// CONFIRM that write_blocks writes exactly 1 block's worth of bytes from the buffer
/**
 * Copies the directory_table into a character array and returns a pointer to the start of it
 */
char* convert_directory_table_to_char_array() {
    char dir_tbl_as_char_array[sizeof(directory_table)];
    memcpy(dir_tbl_as_char_array, directory_table, sizeof(directory_table));
    return dir_tbl_as_char_array;
}

/**
 * Flush root directory
 */
void flush_root_directory() {
    // We need to flush the entire root directory to disk. This means we have to get the blocks pointed to
    // by the root directory's inode one by one, and incrementally write the directory_table to these blocks
    char* buf = convert_directory_table_to_char_array();
    for (int i = 0, j = 0; i < ROOT_DIRECTORY_SIZE_IN_BLOCKS; i++, j += BLOCK_SZ) {
        int block_no = get_block_number_containing_byte_for_inode(0, j);
        if (block_no !=  -1) {
            write_blocks(block_no, 1, buf + j);
        } else {
            printf("Attempted to access memory outside of the scope of the directory table - root directory flush failed.\n");
            break;
        }
    }
}

/****************************************************
 *********************** API ************************
 ****************************************************/

void mksfs(int fresh) {
    if (fresh) {
        printf("making new file system\n");

        init_fresh_disk(KEITHS_DISK, BLOCK_SZ, NUM_BLOCKS);

        /**
         * SUPERBLOCK
         */
        // create super block
        init_superblock();
        // Use first block for the superblock
        force_set_index(0);
        write_blocks(0, 1, &sb);

        /**
         * FREE BIT MAP
         * Reserve blocks for the free bit map
         */
        for (int i = 0; i < NUM_BIT_MAP_BLOCKS; i++) {
            get_index();
        }

        /**
         * INODE TABLE
         */
        // Reserve blocks for the inode table
        for (int i = 0; i < NUM_INODE_BLOCKS; i++) {
            get_index();
        }
        // Set the first entry in the inode table to be an inode_t for the root directory
        init_root_dir_inode();
        // write inode table to disk
        write_blocks(1+NUM_BIT_MAP_BLOCKS, sb.inode_table_len, inode_table);

        // Write free bit map to disk
        write_blocks(1, NUM_BIT_MAP_BLOCKS,  free_bit_map);

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
 * Path is the name of the file
 * Returns the file size, in bytes, if the file exists, and -1 otherwise
 */
int sfs_getfilesize(const char* path) {
    int index = get_directory_index_for_file_with_name(path);
    if (index != -1) {
        // The file exists, so it's inode must be in memory, assuming we've ensured to save it to memory...
        return inode_table[directory_table[index].inode_no].size;
    }
    return -1;
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

    // If file exists, then open it in append mode
    // else,
    // 1. allocate and initialize an inode. Figure out an empty inode slot in the inode table
    // and save the inode to this slot.
    // 2. Write the mapping between the inode and the file name in the root directory: write it to the copy in memory
    // and then write the copy back to disk
    // 3. Set the file size to zero

    // Search the directory for the file
    int index = get_directory_index_for_file_with_name(name);
    if (index != -1) {
        // File exists - get it's inode number
        int inode_no = directory_table[index].inode_no;
        // Search for the inode number in the fd table. If already in the fd table, then it is already open, so we just return its index in the fd table
        int fd = get_fd_for_file_with_inode(inode_no);
        if (fd != -1) {
            // File aleady open
            // Changing the rw pointer could be problematic, so we just return the fd and call it a day
            return fd;
        } else {
            // Open file in APPEND mode (hence passing the size of the file as the rwptr)
            // Notice: Simply setting the fd_table entry for the file effectively "opens" it
            fd = add_to_fd_table(inode_no, inode_table[inode_no].size);
            return fd;
        }
    } else {
        // File does not exist
        // Get an inode for the file
        int inode_no = get_next_available_inode();
        if (inode_no != -1) {
            // Initialize the inode
            initialize_new_inode(inode_no);
            // Add the file to the fd_table
            int fd = add_to_fd_table(inode_no, 0);
            if (fd != -1) {
                // Add the file to the root directory
                int index = add_to_root_directory(inode_no, name);
            }
            return fd;
        } else {
            printf("Error: Cannot create new file as no more inodes are available!\n");
            return -1;
        }
    }
}

/**
 * Closes a file. All this involves is removing the entry at index fileID
 * from the file descriptor table
 * Returns 0 on success and a negative number otherwise
 */
int sfs_fclose(int fileID){
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

    file_descriptor_t* f = &fdt[fileID];
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

    // Determine if the write will increase the size of the file
    // If so,
    // 1. Allocate disk blocks (mark them as allocated in free block map)
    // 2. Modify file's inode to point to these blocks
    // 3. Write the data in buf to these blocks, which are in mem
    // 4. Flush all the data to disk
    file_descriptor_t* f = &fdt[fileID];
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
 * to loc. i.e. change the rwptr property of the file_descriptor_t struct
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
