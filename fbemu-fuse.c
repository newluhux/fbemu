/*

Plan9 style driver, not use ioctl. use 'ctl' interface.

*/

#define FUSE_USE_VERSION 35
#include <fcntl.h>
#include <fuse.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <time.h>
#include <unistd.h>

#define INIT_XRES 800
#define INIT_YRES 600
#define INIT_BITS 32

#define DEVICE_FILENAME  "data"
#define CONTROL_FILENAME "ctl"

static uint8_t *fbemu_buf = NULL;
static struct fb_fix_screeninfo fbemu_finfo;
static struct fb_var_screeninfo fbemu_vinfo;

#define CMD_RESIZE "resize"

static void fbemu_resize(unsigned short x, unsigned short y,
			 unsigned short bits)
{
	fbemu_vinfo.xres = x;
	fbemu_vinfo.yres = y;
	fbemu_vinfo.bits_per_pixel = bits;
	fbemu_finfo.smem_len = x * y * bits / 8;
	fbemu_finfo.line_length = x * bits / 8;
	fbemu_buf = realloc(fbemu_buf, fbemu_finfo.smem_len);
}

static void *fbemu_init(struct fuse_conn_info *conn, struct fuse_config *cfg)
{
	(void)conn;
	(void)cfg;
	return NULL;
}

static int fbemu_getattr(const char *path, struct stat *stbuf,
			 struct fuse_file_info *fi)
{
	(void)fi;
	int res = 0;
	char *filename = (char *)path;
	filename++;
	memset(stbuf, 0, sizeof(struct stat));
	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();
	stbuf->st_atime = stbuf->st_mtime = time(NULL);
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0550;
		stbuf->st_nlink = 2;
	} else if (strcmp(filename, DEVICE_FILENAME) == 0) {
		stbuf->st_mode = S_IFREG | 0660;
		stbuf->st_nlink = 1;
		stbuf->st_size = fbemu_finfo.smem_len;
	} else if (strcmp(filename, CONTROL_FILENAME) == 0) {
		stbuf->st_mode = S_IFREG | 0660;
		stbuf->st_nlink = 1;
		stbuf->st_size = 4096; // max of len
	} else {
		res = -ENOENT;
	}

	return res;
}

static int fbemu_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi,
			 enum fuse_readdir_flags flags)
{
	(void)path;
	(void)offset;
	(void)fi;
	(void)flags;
	(void)buf;

	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);
	filler(buf, DEVICE_FILENAME, NULL, 0, 0);
	filler(buf, CONTROL_FILENAME, NULL, 0, 0);
	return 0;
}

static int fbemu_open(const char *path, struct fuse_file_info *fi)
{
	(void)fi;
	int res = -ENOENT;
	char *filename = (char *)path;
	filename++;
	if (strcmp(filename, DEVICE_FILENAME) == 0) {
		res = 0;
	} else if (strcmp(filename, CONTROL_FILENAME) == 0) {
		res = 0;
	}
	return res;
}

static int fbemu_read_control(char *buf, size_t size) {
	snprintf(buf,size,"xres: %d\nyres: %d\nbits: %d\nmemlen: %d\nlinelen: %d\n",
			fbemu_vinfo.xres, fbemu_vinfo.yres,
			fbemu_vinfo.bits_per_pixel ,fbemu_finfo.smem_len,
			fbemu_finfo.line_length);
	return strlen(buf);
}

static int fbemu_read_device(char *buf, size_t size, off_t offset)
{
	size_t nr = size;
	if (offset < fbemu_finfo.smem_len) {
		nr = fbemu_finfo.smem_len - offset;
		if (nr > size) {
			nr = size;
		}
	} else {
		nr = 0;
	}
	if (nr > 0)
		memcpy(buf,fbemu_buf+offset,nr);
	return nr;
}

static int fbemu_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	(void)fi;
	(void)offset;
	(void)buf;
	(void)size;
	char *filename = (char *)path;
	filename++;
	if (strcmp(filename, DEVICE_FILENAME) == 0) {
		return fbemu_read_device(buf, size, offset);
	} else if (strcmp(filename, CONTROL_FILENAME) == 0) {
		return fbemu_read_control(buf, size);
	}
	return -EINVAL;
}

static int fbemu_write_control(const char *buf, size_t size)
{
	int res = size;
	char *tempbuf = strndup(buf, size);
	if (strstr(tempbuf, CMD_RESIZE) == tempbuf) {
		unsigned short x, y, bits;
		strtok(tempbuf, " \n");	// skip command
		x = atoi(strtok(NULL, " \n"));	// x
		y = atoi(strtok(NULL, " \n"));	// y
		bits = atoi(strtok(NULL, " \n"));	// bits
		fbemu_resize(x, y, bits);
	} else {
		res = -EINVAL;
	}
	free(tempbuf);
	return res;
}

static int fbemu_write_device(const char *buf, size_t size, off_t offset)
{
        size_t nw = size;
        if (offset < fbemu_finfo.smem_len) {
                nw = fbemu_finfo.smem_len - offset;
		if (nw > size) {
			nw = size;
		}
        } else {
                nw = 0;
	}
        if (nw > 0)
                memcpy(fbemu_buf+offset,buf,nw);
        return nw;
}

static int fbemu_write(const char *path, const char *buf, size_t size,
		       off_t offset, struct fuse_file_info *fi)
{
	(void)fi;
	(void)offset;
	char *filename = (char *)path;
	filename++;
	if (strcmp(filename, DEVICE_FILENAME) == 0) {
		return fbemu_write_device(buf, size, offset);
	} else if (strcmp(filename, CONTROL_FILENAME) == 0) {
		return fbemu_write_control(buf, size);
	}
	return -EINVAL;
}

static const struct fuse_operations fbemu_oper = {
	.init = fbemu_init,
	.getattr = fbemu_getattr,
	.readdir = fbemu_readdir,
	.open = fbemu_open,
	.read = fbemu_read,
	.write = fbemu_write,
};

int main(int argc, char *argv[])
{
	fbemu_resize(INIT_XRES, INIT_YRES, INIT_BITS);
	return (fuse_main(argc, argv, &fbemu_oper, NULL));
}
