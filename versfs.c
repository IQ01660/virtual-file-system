/**
 * \file versfs.c
 * \date November 2020
 * \author Scott F. Kaplan <sfkaplan@amherst.edu>
 * 
 * A user-level file system that maintains, within the storage directory, a
 * versioned history of each file in the mount point.
 *
 * FUSE: Filesystem in Userspace
 * Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
 * Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>
 *
 * This program can be distributed under the terms of the GNU GPL.
 */

#define FUSE_USE_VERSION 26

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

static char* storage_dir = NULL;
static char  storage_path[256];


char* prepend_storage_dir (char* pre_path, const char* path) {
  strcpy(pre_path, storage_dir);
  strcat(pre_path, path);
  return pre_path;
}


static int vers_getattr(const char *path, struct stat *stbuf)
{
	int res;
	
	path = prepend_storage_dir(storage_path, path);
	res = lstat(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int vers_access(const char *path, int mask)
{
	int res;

	path = prepend_storage_dir(storage_path, path);
	res = access(path, mask);
	if (res == -1)
		return -errno;

	return 0;
}

static int vers_readlink(const char *path, char *buf, size_t size)
{
	int res;

	path = prepend_storage_dir(storage_path, path);
	res = readlink(path, buf, size - 1);
	if (res == -1)
		return -errno;

	buf[res] = '\0';
	return 0;
}


static int vers_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;

	path = prepend_storage_dir(storage_path, path);
	dp = opendir(path);
	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0))
			break;
	}

	closedir(dp);
	return 0;
}

static int vers_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res;

	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	path = prepend_storage_dir(storage_path, path);
	if (S_ISREG(mode)) {
		res = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(path, mode);
	else
		res = mknod(path, mode, rdev);
	if (res == -1)
		return -errno;

	return 0;
}

static int vers_mkdir(const char *path, mode_t mode)
{
	int res;

	path = prepend_storage_dir(storage_path, path);
	res = mkdir(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int vers_unlink(const char *path)
{
	int res;

	const char *filename = path; // with the slash in front of it
	const char *vers_folder_name = "/.vers"; // the hidden versiion control folder
	const char *tail = "_hist"; // the tail of the history folder of each file

	path = prepend_storage_dir(storage_path, path);
	res = unlink(path);
	if (res == -1)
		return -errno;

	// ================================
	// ----------- MY CODE ------------
	
	// getting the path to the history folder of the file
	char hist_folder_path[256];

	strcpy(hist_folder_path, storage_dir);
	strcat(hist_folder_path, vers_folder_name);
	strcat(hist_folder_path, filename);
	strcat(hist_folder_path, tail);

	// and to next_vers.txt
	// to get the version to iterate two
        const char *next_vers_name = "/next_vers.txt";
        char next_vers_path[256]; // the path to next_vers.txt wll be here

	strcpy(next_vers_path, hist_folder_path);
	strcat(next_vers_path, next_vers_name);

	// getting the version as a string via reading the file
	int nextv;
	int resv;
        
	nextv = open(next_vers_path, O_CREAT | O_RDWR, S_IRWXU);
	
	if (nextv == -1)
		return -errno;

	// now read the file
	char next_vers_buf[3]; // the buffer that will store the next version
	resv = pread(nextv, next_vers_buf, 3, 0);

	if (resv == -1)
		return -errno;
	
	// casting the version into an int
	// THIS SHOULD BE USE WHLE ITERATING THROUGH FLES IN HIST FOLDER
	int next_version_num;
	sscanf(next_vers_buf, "%d", &next_version_num);

	close(nextv);

	// --------------------------------
	// ITERATNG THROUGH THE FILES IN HIST AND UNLINKING THEM
	
	int i;

	for (i = 0; i < next_version_num; i += 1)
	{
	  // casting the i into a string
	  char vers_string_null [3]; // size is three here only due to null character

	  // getting rid of a possible null character at the end
	  sprintf(vers_string_null, "%d", i);


	  char snap_file_path [256];
	  const char *suffix = ",";
	  
	  strcpy(snap_file_path, hist_folder_path);
	  strcat(snap_file_path, filename);
	  strcat(snap_file_path, suffix);
	  strcat(snap_file_path, vers_string_null);

	  // unlink the file
	  int res_snap;

	  res_snap = unlink(snap_file_path);

	  if (res_snap == -1)
		  return -errno;

	}

	// DELETING THE next_vers.txt
	unlink(next_vers_path);

	// DELETING THE foo.txt_hist folder
	rmdir(hist_folder_path);
	
	// ================================

	return 0;
}

static int vers_rmdir(const char *path)
{
	int res;

	path = prepend_storage_dir(storage_path, path);
	res = rmdir(path);
	if (res == -1)
		return -errno;

	return 0;
}

static int vers_symlink(const char *from, const char *to)
{
	int res;
	char storage_from[256];
	char storage_to[256];

	prepend_storage_dir(storage_from, from);
	prepend_storage_dir(storage_to,   to  );
	res = symlink(storage_from, storage_to);
	if (res == -1)
		return -errno;

	return 0;
}

static int vers_rename(const char *from, const char *to)
{
	int res;
	char storage_from[256];
	char storage_to[256];

	prepend_storage_dir(storage_from, from);
	prepend_storage_dir(storage_to,   to  );
	res = rename(storage_from, storage_to);
	if (res == -1)
		return -errno;

	return 0;
}

static int vers_link(const char *from, const char *to)
{
	int res;
	char storage_from[256];
	char storage_to[256];

	prepend_storage_dir(storage_from, from);
	prepend_storage_dir(storage_to,   to  );
	res = link(storage_from, storage_to);
	if (res == -1)
		return -errno;

	return 0;
}

static int vers_chmod(const char *path, mode_t mode)
{
	int res;

	path = prepend_storage_dir(storage_path, path);
	res = chmod(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int vers_chown(const char *path, uid_t uid, gid_t gid)
{
	int res;

	path = prepend_storage_dir(storage_path, path);
	res = lchown(path, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

static int vers_truncate(const char *path, off_t size)
{
	int res;

	path = prepend_storage_dir(storage_path, path);
	res = truncate(path, size);
	if (res == -1)
		return -errno;

	return 0;
}

#ifdef HAVE_UTIMENSAT
static int vers_utimens(const char *path, const struct timespec ts[2])
{
	int res;

	/* don't use utime/utimes since they follow symlinks */
	path = prepend_storage_dir(storage_path, path);
	res = utimensat(0, path, ts, AT_SYMLINK_NOFOLLOW);
	if (res == -1)
		return -errno;

	return 0;
}
#endif

static int vers_open(const char *path, struct fuse_file_info *fi)
{
	int res;

	path = prepend_storage_dir(storage_path, path);
	res = open(path, fi->flags);

	if (res == -1)
		return -errno;

	close(res);

	return 0;
}

static int vers_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	int fd;
	int res;
	int i;
	char temp_buf[size];

	(void) fi;
	path = prepend_storage_dir(storage_path, path);
	fd = open(path, O_RDONLY);
	if (fd == -1)
		return -errno;

	res = pread(fd, temp_buf, size, offset);
	if (res == -1)
		res = -errno;

	// Move data from temporary buffer into provided one.
	for (i = 0; i < size; i += 1) {
	  buf[i] = temp_buf[i];
	}

	close(fd);
	return res;
}

static int vers_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	int fd;
	int res;
	int i;
	char temp_buf[size];

	// storing the init path for later
	const char *filename = path; // with the slash in front of it

	(void) fi;
	path = prepend_storage_dir(storage_path, path);
	fd = open(path, O_WRONLY);

	// ================================
	// ----------- MY CODE ------------
	


	// CHECK IF .vers EXISTS -> if no then create it
	// get its full path first
	const char *vers_folder_name = "/.vers";
	char vers_folder_path[256]; // the full path will be here
	
	strcpy(vers_folder_path, storage_dir);
	strcat(vers_folder_path, vers_folder_name);

	DIR *vf = opendir(vers_folder_path);
	if (vf == NULL)
	{
	  // creating the .vers directory
	  mkdir(vers_folder_path, S_IRWXU | S_IRGRP | S_IROTH);
	}

	// CHECK IF .vers/filename.txt_hist exists -> if no then create it
	// and put next_vers.txt into it
	// and write a value of 0 into next_vers.txt
	
	// get the full path to e.g. foo.txt_hist
	const char *tail = "_hist";
	char hist_folder_path[256];

	strcpy(hist_folder_path, storage_dir);
	strcat(hist_folder_path, vers_folder_name);
	strcat(hist_folder_path, filename);
	strcat(hist_folder_path, tail);

	// and to next_vers.txt
        const char *next_vers_name = "/next_vers.txt";
        char next_vers_path[256]; // the path to next_vers.txt wll be here

	strcpy(next_vers_path, hist_folder_path);
	strcat(next_vers_path, next_vers_name);


	DIR *ht = opendir(hist_folder_path);
	if (ht == NULL)
	{
	  // create the hist directory
	  mkdir(hist_folder_path, S_IRWXU | S_IRGRP | S_IROTH);

	  // put next_vers.txt into it
	  
	  int nextv;
	  int res;
	  // this will create the file and open it up
	  nextv = open(next_vers_path, O_CREAT | O_RDWR , S_IRWXU);

	  if (nextv == -1)
		  return -errno;

	  // writing "0" into the file
	  char vers_buf[1];

	  vers_buf[0] = '0';

	  res = pwrite(nextv, vers_buf, 1, 0);

	  if (res == -1)
		  return -errno;

	  close(nextv);

	}

	// READING FROM next_vers.txt TO FIND THE VERSION
	// OF THE FILE TO BE CREATED
	
	// first open up the file - next_vers.txt
	int nextv;
	int resv;
        
	nextv = open(next_vers_path, O_CREAT | O_RDWR, S_IRWXU);
	
	if (nextv == -1)
		return -errno;

	// now read the file
	char next_vers_buf[3]; // the buffer that will store the next version
	resv = pread(nextv, next_vers_buf, 3, 0);

	if (resv == -1)
		return -errno;

	// now store that value in a variable to use LATER as a string
	char next_version[3];
	next_version[0] = next_vers_buf[0];
	next_version[1] = next_vers_buf[1];
	next_version[2] = next_vers_buf[2];

	// but now update the value of the next version in the next_vers.txt by incrementing it
	
	// first convert a char to an int
	//int version_num = (int) next_vers_buf[0] - (int) '0';
	int version_num;
	sscanf(next_vers_buf, "%d", &version_num);

	// store the version before this one for LATER
	// this is needed if an append operation is done
	char prev_vers[3];

	if (version_num != 0)
	{
	  int prev_vers_num = version_num - 1;
	  sprintf(prev_vers, "%d", prev_vers_num);
	}

	// incrementing the version number
	version_num += 1; 

	// converting it back to char ARRAY
	char update_vers_buf [3]; // size is three here only due to null character

        
	sprintf(update_vers_buf, "%d", version_num);

	// write version back into next_vers.txt
	
	int resv_wr;
	resv_wr = pwrite(nextv, update_vers_buf, 2, 0);
	
	if (resv_wr == -1)
		return -errno;

	close(nextv);

	
	// CREATING A SNAPSHOT FILE with a suffix in .vers/foo.txt_hist/foo.txt,v
	// getting the path to the file
	char snap_file_path[256];
	const char *suffix = ",";
	strcpy(snap_file_path, hist_folder_path);
	strcat(snap_file_path, filename);
	strcat(snap_file_path, suffix);
	strcat(snap_file_path, next_version);

	int snap;
	int snap_res;
	
	snap = open(snap_file_path, O_CREAT | O_RDWR, S_IRWXU);

	if (snap == -1)
		return -errno;

	// IF WE ARE APPENDING TO A FILE THEN MAKE SMTH DIFFERENT WITH A temp_buf
	// checking is the offset from which to be writen is 0
	if (offset == 0)
	{
	
	  // updating the temp_buf
	  for (i = 0; i < size; i += 1) {
	    temp_buf[i] = buf[i];
	  }

	  snap_res = pwrite(snap, temp_buf, size, offset);
	}

	else
	{
	  // read the previous version of file into temp_buf
	  int prev_snap;
	  int prev_snap_rd;
	
	  // getting the path to the prev snap
	  char prev_snap_path[256];
	  strcpy(prev_snap_path, hist_folder_path);
	  strcat(prev_snap_path, filename);
	  strcat(prev_snap_path, suffix);
	  strcat(prev_snap_path, prev_vers);

	  // open the file
	  prev_snap = open(prev_snap_path, O_CREAT | O_RDWR, S_IRWXU);

	  if (prev_snap == -1)
		  return -errno;

	  // read the file
	  prev_snap_rd = pread(prev_snap, temp_buf, offset, 0);
	  
	  if (prev_snap_rd == -1)
		  return -errno;

	  // adding the new chars to the text's buffer
	  for (i = offset; i < offset + size; i += 1)
	  {
	    temp_buf[i] = buf[i - offset];
	  }

	  snap_res = pwrite(snap, temp_buf, offset + size, 0);
	  
	}

	if (snap_res == -1)
		return -errno;

	close(snap);
	

	//=====================

	if (fd == -1)
		return -errno;

	res = pwrite(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	close(fd);
	return res;
}

static int vers_statfs(const char *path, struct statvfs *stbuf)
{
	int res;

	path = prepend_storage_dir(storage_path, path);
	res = statvfs(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int vers_release(const char *path, struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) fi;
	return 0;
}

static int vers_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

#ifdef HAVE_POSIX_FALLOCATE
static int vers_fallocate(const char *path, int mode,
			off_t offset, off_t length, struct fuse_file_info *fi)
{
	int fd;
	int res;

	(void) fi;

	if (mode)
		return -EOPNOTSUPP;

	path = prepend_storage_dir(storage_path, path);
	fd = open(path, O_WRONLY);
	if (fd == -1)
		return -errno;

	res = -posix_fallocate(fd, offset, length);

	close(fd);
	return res;
}
#endif

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int vers_setxattr(const char *path, const char *name, const char *value,
			size_t size, int flags)
{
	path = prepend_storage_dir(storage_path, path);
	int res = lsetxattr(path, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
}

static int vers_getxattr(const char *path, const char *name, char *value,
			size_t size)
{
	path = prepend_storage_dir(storage_path, path);
	int res = lgetxattr(path, name, value, size);
	if (res == -1)
		return -errno;
	return res;
}

static int vers_listxattr(const char *path, char *list, size_t size)
{
	path = prepend_storage_dir(storage_path, path);
	int res = llistxattr(path, list, size);
	if (res == -1)
		return -errno;
	return res;
}

static int vers_removexattr(const char *path, const char *name)
{
	path = prepend_storage_dir(storage_path, path);
	int res = lremovexattr(path, name);
	if (res == -1)
		return -errno;
	return 0;
}
#endif /* HAVE_SETXATTR */

static struct fuse_operations vers_oper = {
	.getattr	= vers_getattr,
	.access		= vers_access,
	.readlink	= vers_readlink,
	.readdir	= vers_readdir,
	.mknod		= vers_mknod,
	.mkdir		= vers_mkdir,
	.symlink	= vers_symlink,
	.unlink		= vers_unlink,
	.rmdir		= vers_rmdir,
	.rename		= vers_rename,
	.link		= vers_link,
	.chmod		= vers_chmod,
	.chown		= vers_chown,
	.truncate	= vers_truncate,
#ifdef HAVE_UTIMENSAT
	.utimens	= vers_utimens,
#endif
	.open		= vers_open,
	.read		= vers_read,
	.write		= vers_write,
	.statfs		= vers_statfs,
	.release	= vers_release,
	.fsync		= vers_fsync,
#ifdef HAVE_POSIX_FALLOCATE
	.fallocate	= vers_fallocate,
#endif
#ifdef HAVE_SETXATTR
	.setxattr	= vers_setxattr,
	.getxattr	= vers_getxattr,
	.listxattr	= vers_listxattr,
	.removexattr	= vers_removexattr,
#endif
};

int main(int argc, char *argv[])
{
	umask(0);
	if (argc < 3) {
	  fprintf(stderr, "USAGE: %s <storage directory> <mount point> [ -d | -f | -s ]\n", argv[0]);
	  return 1;
	}
	storage_dir = argv[1];
	char* mount_dir = argv[2];
	if (storage_dir[0] != '/' || mount_dir[0] != '/') {
	  fprintf(stderr, "ERROR: Directories must be absolute paths\n");
	  return 1;
	}
	fprintf(stderr, "DEBUG: Mounting %s at %s\n", storage_dir, argv[2]);
	int short_argc = argc - 1;
	char* short_argv[short_argc];
	short_argv[0] = argv[0];
	for (int i = 2; i < argc; i += 1) {
	  short_argv[i - 1] = argv[i];
	}
	return fuse_main(short_argc, short_argv, &vers_oper, NULL);
}
