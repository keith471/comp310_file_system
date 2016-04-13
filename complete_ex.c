#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include "disk_emu.h"
#include "sfs_api.h"

#define MAXFILENAME 30

FILE* log_fd;

static int xmp_getattr(const char *path, struct stat *stbuf)
{
    int res = 0;
    int size;

    fprintf(log_fd, "xmp_getattr:: path = %s\n", path);
    fflush(log_fd);

    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        fprintf(log_fd, "xmp_getattr 1\n");
        fflush(log_fd);
    } else if((size = sfs_getfilesize(path)) != -1) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = size;
        fprintf(log_fd, "xmp_getattr 2\n");
        fflush(log_fd);
    } else {
        res = -ENOENT;
        fprintf(log_fd, "xmp_getattr 3\n");
        fflush(log_fd);
    }
    return res;
}
static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
    char file_name[MAXFILENAME];

    fprintf(log_fd, "xmp_readdir:: path = %s\n", path);
    fflush(log_fd);

    if (strcmp(path, "/") != 0)
            return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    while(sfs_getnextfilename(file_name)) {
        filler(buf, file_name, NULL, 0);
    }

    return 0;
}

static int xmp_unlink(const char *path)
{
	int res;
        char filename[MAXFILENAME];

        strcpy(filename, &path[1]);
	res = sfs_remove(filename);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi)
{
	int res;
        char filename[MAXFILENAME];

        strcpy(filename, &path[1]);
        fprintf(log_fd, "xmp_open:: filename = %s\n", filename);
        fflush(log_fd);
	res = sfs_fopen(filename);
	if (res == -1) {
      printf("Open error\n");
      return -errno;
  }

  sfs_fclose(res);
	return 0;
}
static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	int fd;
	int res;

  char filename[MAXFILENAME];

  strcpy(filename, &path[1]);
  fprintf(log_fd, "xmp_read:: filename = %s\n", filename);
  fflush(log_fd);
	fd = sfs_fopen(filename);
	if (fd == -1) {
    fprintf(log_fd, "open error in xmp_read\n");
    fflush(log_fd);
    return -errno;
  }
  if(sfs_fseek(fd, offset) == -1) {
    return -errno;
  }
	res = sfs_fread(fd, buf, size);
	if (res == -1) {
    fprintf(log_fd, "read error in xmp_read\n");
    fflush(log_fd);
    res = -errno;
  }
  fprintf(log_fd, "Data read: %s\n", buf);
  fflush(log_fd);
	sfs_fclose(fd);
	return res;
}
static int xmp_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	int fd;
	int res;

	char filename[MAXFILENAME];

  strcpy(filename, &path[1]);
  fd = sfs_fopen(filename);
	if (fd == -1) {
      fprintf(log_fd, "xmp_write::why is this not working??\n");
      fflush(log_fd);
      return -errno;
  }
  fprintf(log_fd, "xmp_write:: filename = %s\n", filename);
  fflush(log_fd);
	res = sfs_fwrite(fd, buf, size);
	if (res == -1) {
    res = -errno;
  }
	sfs_fclose(fd);
	return res;
}
static int xmp_truncate(const char *path, off_t size)
{
        char filename[MAXFILENAME];
        int fd;

        strcpy(filename, &path[1]);

        fprintf(log_fd, "xmp_truncate:: filename = %s\n", path);
        fflush(log_fd);

        fd = sfs_remove(filename);
	if (fd == -1)
		return -errno;

        fd = sfs_fopen(filename);
        sfs_fclose(fd);
        return 0;
}
static int xmp_access(const char *path, int mask)
{
        fprintf(log_fd, "xmp_access:: pathname = %s\n", path);
        fflush(log_fd);
	return 0;
}
static int xmp_mknod(const char *path, mode_t mode, dev_t rdev)
{
        fprintf(log_fd, "xmp_mknod:: pathname = %s\n", path);
        fflush(log_fd);
	return 0;
}
static int xmp_create (const char *path, mode_t mode, struct fuse_file_info *fp)
{
    char filename[MAXFILENAME];
    int fd;

    strcpy(filename, &path[1]);

    fprintf(log_fd, "xmp_create:: filename = %s\n", path);
    fflush(log_fd);

    fd = sfs_fopen(filename);
    sfs_fclose(fd);

    return 0;
}
static struct fuse_operations xmp_oper = {
	.getattr = xmp_getattr,
	.readdir = xmp_readdir, //done
	.mknod = xmp_mknod,
	.unlink = xmp_unlink, //done
	.truncate = xmp_truncate,
	.open = xmp_open, //done
	.read = xmp_read, //done
	.write = xmp_write, //done
  .access = xmp_access,
  .create = xmp_create,
//	.release = sfs_fclose,
};

int main(int argc, char *argv[])
{
	mksfs(1);
  log_fd = fopen("log.txt", "w");

  if(log_fd == NULL) {
      perror("Error");
      return 0;
  }

	return fuse_main(argc, argv, &xmp_oper, NULL);
}
