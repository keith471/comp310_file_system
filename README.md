# File systems assignment

## Instructions on running with fuse
1. Clean with `make clean`
2. Compile with `make all`. This produces the executable Keith_Strickling_sfs
3. Create a directory, e.g. `mkdir /tmp/test/`
4. Run the executable and pass the directory as a parameter, e.g. `./Keith_Strickling_sfs /tmp/test/`

## Limitations
1. The number of inodes is fixed, as is the number of blocks on disk.
2. This implementation assumes that there is always a free block on disk.
3. File names are limited in length. You can modify this length in sfs_api.h - MAXFILENAME. This length includes the file extension.
4. The null terminator of a string is not removed when writing to the middle of a file. i.e. If we write "Dog\0" to the middle of the file, then the null terminator will be written as well.
5. This implementation does not permit open files to be removed. They must be closed first.
6. This implementation does not support sparse files.

## Other things to note
1. You can edit the files with vim. However, note that while editing with vim, some strange stuff happens. It appears that the file size is repeatedly increased by a large amount and block are allocated while you edit. But, when you save the changes, whatever crazy-big file was allocated is removed and you can still access the data of your file as expected.
2. If you change the block size without modifying other constants defined in sfs_api.h, you may run into problems. For example, a seg fault will occur if you decrease the block size to 64 bytes while leaving all other constants. This is because the root directory will require a greater number of blocks than are permitted to a single file/directory with such small a block size, and the root directory allocation will attempt to access memory it should not have access to.
