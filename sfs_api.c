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

// For use with sfs_getnextfilename
int next_dir_index;

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
 * Initializes the global "next_dir_index" to the first used directory table entry
 */
initialize_next_dir_index() {
    next_dir_index = get_next_filled_directory_entry_starting_at(0);
}

/**
 * Updates the value of the global variable next_dir_index
 * Returns 1 if there is another file in the directory and 0 otherwise
 */
int update_next_dir_index() {
    next_dir_index = get_next_filled_directory_entry_starting_at(next_dir_index + 1);
    if (next_dir_index == -1) {
        return 0;
    } else {
        return 1;
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
 * Iterates through the directory table starting at index "index" and returns the index of the first occupied entry,
 * or -1 if no occupied entries after the given index
 */
int get_next_filled_directory_entry_starting_at(int index) {
    if (index >= MAX_DIRECTORY_ENTRIES) {
        printf("Error: index out of bounds for directory_table.\n");
        return -1;
    }
    for (int i = index; i < MAX_DIRECTORY_ENTRIES; i++) {
        if (directory_table[i].inode_no != 0) {
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
    if (byte_no < 0) {
        printf("Error: Attempting to access a byte before the start of the file.\n");
        return -1;
    }
    if (inode.size < byte_no + 1) {
        printf("Error: Attempting to access a byte beyond the scope of the file.\n");
        return -1;
    }

    // We can take the byte_no and determine whether we want the 0th, 1st, 2nd, 3rd, 4th, ..., nth block of the file
    // i.e. we can determine which pointer we want
    int nth = byte_no / BLOCK_SZ;

    return get_block_number_corresponding_to_nth_block_for_file(inode_no, nth);
}

/**
 * Takes the inode number of a file, and a number corresponding to which block of the file
 * we'd like to access, and returns the block number where that block is located on disk,
 * or -1 if the file has fewer than nth+1 blocks
 */
int get_block_number_corresponding_to_nth_block_for_file(int inode_no, int nth) {
    inode_t inode = inode_table[inode_no];
    // Error checking
    if (nth < 0) {
        printf("Error: There are no negative sequential block numbers.\n");
        return -1;
    }
    if (nth > (inode.size / BLOCK_SZ)) {
        printf("Error: Attempting to access a block the file does not have.\n");
        return -1;
    }
    // Error check passed. Proceeding.
    if (nth < NUM_DIRECT_POINTERS) {
        return inode.data_ptrs[nth];
    } else {
        // We need to read the block of number inode.indirect_ptr into memory
        int indirect_ptrs[NUM_INDIRECT_POINTERS];
        read_blocks(inode.indirect_ptr, 1, indirect_ptrs);
        // Now, we need to return the (nth - NUM_DIRECT_POINTERS)th pointer in the indirect_ptrs array
        return indirect_ptrs[nth - NUM_DIRECT_POINTERS];
    }
}

/**
 * Takes the number of a byte in a file (could be any file), and determines whether this byte
 * is in the 0th, 1st, 2nd, 3rd, ..., nth block of the file and returns this number
 */
int get_sequential_block_number_containing_byte(int byte_no) {
    return byte_no / BLOCK_SZ;
}

/**
 * Takes a number corresponding to the nth consecutive block of the file with the given inode,
 * and determines if the file has such a block or not. Returns 1 if it does, and 0 if not.
 */
int file_has_nth_block(int inode_no, int nth) {
    // Need to check for the case that the inode size is zero, as if it is then we know the file
    // is empty and therefore has no blocks allocated to it yet
    if (inode_table[inode_no].size / BLOCK_SIZE < nth || inode.size == 0) {
        return 0;
    } else {
        return 1;
    }
}

/**
 * Takes an inode number corresponding to a file and allocates the file's nth block.
 * Updates the file's inode appropriately, but does NOT write it back to disk
 * Returns the block number of the allocated block, or -1 if the file cannot have an nth block
 */
int allocate_nth_block_for_file_with_inode(int inode_no, int nth) {
    // Error checking
    if (nth < 0) {
        printf("Error: Cannot allocate a negative block number.\n");
        return -1;
    }
    if (nth > MAX_BLOCKS_PER_FILE - 1) {
        printf("Error: The file has already consumed the maximum allowable number of blocks.\n");
        return -1;
    }
    // Get new block
    int block_no = get_index();
    // Update inode
    if (nth < NUM_DIRECT_POINTERS) {
        inode_table[inode_no].data_ptrs[nth] = block_no;
    } else if (nth == NUM_DIRECT_POINTERS) {
        // We are allocating the first block that requires use of the inode's indirect pointer,
        // so first we need to allocate a block for the indirect pointer
        int ind_ptrs_block = get_index();
        inode_table[inode_no].indirect_ptr = ind_ptrs_block;
        // Have to write an entire block's worth of data to disk, so create an array of an unnecessarily large size
        int indirect_ptrs[NUM_INDIRECT_POINTERS];
        indirect_ptrs[0] = block_no;
        // Write the indirect_ptrs to disk
        write_blocks(ind_ptrs_block, 1, indirect_ptrs);

    } else {
        // Read the block of indirect pointers for the file into memory, modify it, and write it back
        int indirect_ptrs[NUM_INDIRECT_POINTERS];
        read_blocks(inode_table[inode_no].indirect_ptr, 1, indirect_ptrs);
        indirect_ptrs[nth - NUM_DIRECT_POINTERS] = block_no;
        write_blocks(inode_table[inode_no].indirect_ptr, 1, indirect_ptrs);
    }
    return block_no;
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
    //char* buf = convert_directory_table_to_char_array();
    // Cast directory_table to a char pointer so we can increment it by bytes rather than sizeof(directory_entry_t)
    char* p = (char *) directory_table;
    for (int i = 0, j = 0; i < ROOT_DIRECTORY_SIZE_IN_BLOCKS; i++, j += BLOCK_SZ) {
        int block_no = get_block_number_containing_byte_for_inode(0, j);
        if (block_no !=  -1) {
            write_blocks(block_no, 1, p + j);
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

    // We also need to initialize next_dir_index
    initialize_next_dir_index();

	  return;
}

/***
 * How should this function work?
 Global variable initialized to first file in directory
 Increment each time sfs_getnextfilename is called
 */

/**
 * Copies the name of the next file in the directory into fname
 * Returns 1 if there is another file in the directory, after
 *   the one whose name it just copied into fname
 * Returns 0 if it got the name of the last file in the directory
 */
int sfs_getnextfilename(char *fname) {
    if (next_dir_index == -1) {
        printf("Error: There are no more directory entries. This function should not have been called.\n");
        return 0;
    }
    strcpy(fname, directory_table[next_dir_index].file_name);
    return update_next_dir_index();
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
 * Closes a file. All this involves is resetting the entry at index fileID
 * from the file descriptor table
 * Returns 0 on success and -1 number otherwise
 */
int sfs_fclose(int fileID){
    fd_table[fileID].inode = 0;
    fd_table[fileID].rwptr = 0;
    return 0;
}

/**
 * Read length bytes of the file corresponding to file descriptor
 * table entry fileID into buf, starting with the byte of the file indicated by the file's
 * rwpointer
 * Returns the number of bytes read
 */
int sfs_fread(int fileID, char *buf, int length) {

    // Error checking
    if (length == 0) {
        printf("Error: Why would you try to read 0 bytes?\n");
        return 0;
    }
    if (fd_table[fileID].rwptr + length > inode_table[fd_table[fileID].inode_no].size) {
        printf("Error: Attempting to read past the end of the file.\n");
        return -1;
    }

    // Get the sequential numbers of the first and last blocks we need to read
    int first_block = get_sequential_block_number_containing_byte(fd_table[fileID].rwptr);
    int last_block = get_sequential_block_number_containing_byte(fd_table[fileID].rwptr + length - 1);

    // Allocate a buffer to contain the data for all the blocks we need to read from disk
    char temp_buf[(last_block - first_block + 1)*BLOCK_SZ];

    // Iterate, reading one block at a time, and writing it to temp_buf
    for (int i = first_block; i <= last_block; i++) {
        int block_no = get_block_number_corresponding_to_nth_block_for_file(fd_table[fileID].inode_no, i);
        read_blocks(block_no, 1, temp_buf + (i % first_block) * BLOCK_SZ);
    }

    // Copy the bytes we want from temp_buf into buf
    memcpy(buf, temp_buf + (fd_table[fileID].rwptr % BLOCK_SZ), length);

    // QUESTION: Is this correct?
    // Lastly, we need to increase the rwptr for the file
    /*if (fd_table[fileID].rwptr + length == inode_table[fd_table[fileID].inode_no].size) {
        // Seek to the last byte of the file
        sfs_fseek(fileID, fd_table[fileID].rwptr + length - 1);
    } else {
        // Seek to the first byte after the sequence you just read
        sfs_fseek(fileID, fd_table[fileID].rwptr + length);
    }*/
    // Seek to the first byte after the sequence you just read
    // This is fine because, even if the read took us to the last byte of actual content of the file,
    // there is still one more byte in the file, the null pointer
    sfs_fseek(fileID, fd_table[fileID].rwptr + length);

    return length;
    /*file_descriptor_t* f = &fd_table[fileID];
    inode_t* n = &inode_table[f->inode];
    int block = n->data_ptrs[0];
    read_blocks(block, 1, (void*) buf);
    return 0;*/
}

/**
 * Writes length bytes of buf into the file at index fileID of
 * the file descriptor table, starting at the byte of the current rwpointer
 * as determined from the file descriptor table.
 * This could increase the size of the file.
 * Returns the number of bytes written
 * QUESTION: If I increase the size of the file, then I am actually writing length + 1 bytes,
 * due to the null terminator. Is this a problem?
 */
int sfs_fwrite(int fileID, const char *buf, int length){

    // If writing to the end of the file, write one extra byte at the end with value = null terminator
    // So, we should write length bytes from buf, plus (conditionally) one extra byte for the null terminator
    // We have to include this null terminator as part of the size of the file

    // Basic steps:
    // Get range of blocks that you wish to write. Allocate some if need be. Any allocated blocks do not need to be
    // read into memory as they are fresh and there is nothing in them to read
    // Read these blocks into an array
    // Overwrite relevant part of the array
    // Write the blocks back to memory
    // Update data structures in memory and write them back to disk

    int rwptr = fd_table[fileID].rwptr;
    int inode_no = fd_table[fileID].inode_no;

    // Flags that will be used later
    int extending_file = (rwptr + length > inode_table[inode_no].size);
    int added_blocks = false;

    int first_block = get_sequential_block_number_containing_byte(rwptr);
    int last_block = get_sequential_block_number_containing_byte(rwptr + length); // Checked

    // Allocate a buffer to contain the data for all the blocks we need to read from disk
    char temp_buf[(last_block - first_block + 1)*BLOCK_SZ];

    // Iterate, reading one block at a time, and writing it to temp_buf
    for (int i = first_block; i <= last_block; i++) {
        if (file_has_nth_block(i)) {
            int block_no = get_block_number_corresponding_to_nth_block_for_file(inode_no, i);
            if (block_no == -1) {
                return -1; // error
            }
            read_blocks(block_no, 1, temp_buf + (i % first_block) * BLOCK_SZ);
        } else {
            // Allocate a new block for the file
            // No need to read it, as it doesn't yet contain any file data
            int block_no = allocate_nth_block_for_file_with_inode(inode_no, i);
            if (block_no == -1) {
                return -1; // error
            }
            added_blocks = true;
        }
    }
    // Overwrite part of this block of data by writing length bytes of buf to temp_buf
    // starting at block_data + (rwptr % BLOCK_SZ)
    memcpy(temp_buf + (rwptr % BLOCK_SZ), buf, length);

    // If we are extending the file, then add a new null terminator to the end of the file
    if (extending_file) {
        temp_buf[(rwptr % BLOCK_SZ) + length] = '\0';
        // QUESTION: Do we increment length to indicate that we wrote a new null terminator as well?
    }

    // Write the blocks back to disk!
    for (int i = first_block; i <= last_block; i++) {
        // Could have stored the block numbers in an array or something, but since we are not worried
        // about the most efficient implementation, I don't bother
        int block_no = get_block_number_corresponding_to_nth_block_for_file(inode_no, i);
        write_blocks(block_no, 1, temp_buf + (i % first_block) * BLOCK_SZ);
    }

    // Update the rwpointer for the file
    fd_table[fileID].rwptr += length;

    if (extending_file) {
        // Update the file size, and write the inode table back to disk
        inode_table[inode_no].size = fd_table[fileID].rwptr + 1;
        flush_inode_table();

        // Flush the free bit map if need be
        if (added_blocks) {
            flush_free_bit_map();
        }
        return length + 1;
    }

    return length;

    /*
    if (rwptr + length > inode_table[inode_no].size) {
        // The write will increase the size of the file
        // Determine if the write will require the allocation of additional blocks or not
        // Additional blocks will be required if it is the first write to the file
        // What are the cases where additional blocks are required?
        // 1. If size == 0, then no blocks have been allocated to the file yet
        // 2. If rwptr + length takes us into a new block
        // What is the case where additional blocks are not required?
        // Where (rwptr + length - size) < (BLOCK_SZ - (size % BLOCK_SZ))

        // If we are increasing the size of the file, but don't need additional blocks, then we need to
        // 1. Read all the blocks, starting with the block containing the rwptr and continuing through
        // the last block into memory
        // Get the sequential numbers of the first and last blocks we need to read
        int first_block = get_sequential_block_number_containing_byte(rwptr);
        int last_block = get_sequential_block_number_containing_byte(inode_table[inode_no].size);

        // Allocate a buffer to contain the data for all the blocks we need to read from disk
        char temp_buf[(last_block - first_block + 1)*BLOCK_SZ];

        // Iterate, reading one block at a time, and writing it to temp_buf
        for (int i = first_block; i <= last_block; i++) {
            int block_no = get_block_number_corresponding_to_nth_block_for_file(inode_no, i);
            read_blocks(block_no, 1, temp_buf + (i % first_block) * BLOCK_SZ);
        }
        // 2. Overwrite part of this block of data by writing length bytes of buf to the char array
        // starting at block_data + (rwptr % BLOCK_SZ)
        memcpy(temp_buf + (rwptr % BLOCK_SZ), buf, length);
        // 3. Write a new null terminator to the end of the data
        temp_buf[(rwptr % BLOCK_SZ) + length] = '\0';
        // 4. Write all the blocks back to disk
        for (int i = first_block; i <= last_block; i++) {
            // Could have stored the block numbers in an array or something, but since we are not worried
            // about the most efficient implementation, I don't bother
            int block_no = get_block_number_corresponding_to_nth_block_for_file(inode_no, i);
            write_blocks(block_no, 1, temp_buf + (i % first_block) * BLOCK_SZ);
        }
        // 5. Carry on


    } else {
        // The write is only overwriting current data in the file
    }
    */

    /*file_descriptor_t* f = &fd_table[fileID];
    inode_t* n = &inode_table[fileID];*/
}

/**
 * Move the rwpointer for the file corresponding to fd entry fileID
 * to loc. i.e. change the rwptr property of the file_descriptor_t struct
 * at index fileID of the file descriptor table to loc.
 * Returns 0 if success and -1 if error (i.e. trying to move the rwpointer
 * outside of the bounds of the file)
 */
int sfs_fseek(int fileID, int loc){

    /* Perform error checking:
     * If loc > size, we are moving the pointer past the end of the file. This
     * is not allowed.
     * If loc is negative, this, of course, is not allowed
     */
    if (loc > inode_table[fd_table[fileID].inode_no].size) {
        printf("Error: Attempting to seek past the end of a file.\n");
        return -1;
    } else if (loc < 0) {
        printf("Error: Attempting to seek before the start of a file.\n");
        return -1;
    }
    fd_table[fileID].rwptr = loc;
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


/***************************
 * MARK -  Bitmap helpers
 ***************************/

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
