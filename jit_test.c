
/* sfs_test.c
 *
 * Written by Robert Vincent for Programming Assignment #1.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sfs_api.h"

int main(int argc, char* argv[]) {

    int init_fs;
    if (argc != 2) {
        printf("Error - enter 0 to open an existing file system or 1 to create a new file system\n");
        return 0;
    } else {
        init_fs = atoi(argv[1]);
    }
    mksfs(init_fs);
    if (init_fs) {
        // Writing a new file system

  	    int f = sfs_fopen("some_name.txt");

  	    char my_data[] = "Lazy dog.";
  	    char out_data[1024];
  	    sfs_fwrite(f, my_data, sizeof(my_data));
        char my_data2[] = "Cool cat.";
        sfs_fwrite(f, my_data2, sizeof(my_data2));
  	    sfs_fseek(f, 0);
  	    sfs_fread(f, out_data, sizeof(out_data));
  	    printf("%s\n", out_data);

  	    sfs_fclose(f);
        sfs_remove("some_name.txt");
    } else {
        // Try to read the file that was written when creating a new disk
        int f = sfs_fopen("some_name.txt");
  	    char out_data[1024];
  	    sfs_fseek(f, 0);
  	    sfs_fread(f, out_data, sizeof(out_data)+1);
  	    printf("%s\n", out_data);
  	    sfs_fclose(f);
    }
}
