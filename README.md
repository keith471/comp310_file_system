# File systems assignment

## Limitations
1. The number of inodes is fixed, as is the number of blocks on disk.
2. This implementation assumes that there is always a free block on disk.
3. File names are limited in length. You can modify this length in sfs_api.h - MAXFILENAME. This length includes the file extension.
4. The null terminator of a string is not removed when writing to the middle of a file. i.e. If we write "Dog\0" to the middle of the file, then the null terminator will be written as well.
5. This implementation does not permit open files to be removed. They must be closed first.
