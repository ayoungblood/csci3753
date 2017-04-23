/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  Minor modifications and note by Andy Sayler (2012) <www.andysayler.com>

  Source: fuse-2.8.7.tar.gz examples directory
  http://sourceforge.net/projects/fuse/files/fuse-2.X/

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall `pkg-config fuse --cflags` fusexmp.c -o fusexmp `pkg-config fuse --libs`

  Note: This implementation is largely stateless and does not maintain
        open file handels between open and release calls (fi->fh).
        Instead, files are opened and closed as necessary inside read(), write(),
        etc calls. As such, the functions that rely on maintaining file handles are
        not implmented (fgetattr(), etc). Those seeking a more efficient and
        more complete implementation may wish to add fi->fh support to minimize
        open() and close() calls and support fh dependent functions.

*/

#define FUSE_USE_VERSION 28
#define HAVE_SETXATTR

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

// ANSI colour escapes
#define ANSI_C_BLACK        "\x1b[1;30m"
#define ANSI_C_RED          "\x1b[1;31m"
#define ANSI_C_YELLOW       "\x1b[1;33m"
#define ANSI_C_GREEN        "\x1b[1;32m"
#define ANSI_C_CYAN         "\x1b[1;36m"
#define ANSI_C_BLUE         "\x1b[1;34m"
#define ANSI_C_MAGENTA      "\x1b[1;35m"
#define ANSI_C_WHITE        "\x1b[1;37m"
#define ANSI_RESET          "\x1b[0m"
#define ANSI_BOLD           "\x1b[1m"
#define ANSI_UNDER          "\x1b[4m"

#define KEY_PHRASE_MAX_LEN  80

static int xmp_getattr(const char *path, struct stat *stbuf) {
    int res;

    res = lstat(path, stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_access(const char *path, int mask) {
    int res;

    res = access(path, mask);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size) {
    int res;

    res = readlink(path, buf, size - 1);
    if (res == -1)
        return -errno;

    buf[res] = '\0';
    return 0;
}


static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
               off_t offset, struct fuse_file_info *fi) {
    DIR *dp;
    struct dirent *de;

    (void) offset;
    (void) fi;

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

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev) {
    int res;

    /* On Linux this could just be 'mknod(path, mode, rdev)' but this
       is more portable */
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

static int xmp_mkdir(const char *path, mode_t mode) {
    int res;

    res = mkdir(path, mode);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_unlink(const char *path) {
    int res;

    res = unlink(path);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_rmdir(const char *path) {
    int res;

    res = rmdir(path);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_symlink(const char *from, const char *to) {
    int res;

    res = symlink(from, to);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_rename(const char *from, const char *to) {
    int res;

    res = rename(from, to);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_link(const char *from, const char *to) {
    int res;

    res = link(from, to);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_chmod(const char *path, mode_t mode) {
    int res;

    res = chmod(path, mode);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid) {
    int res;

    res = lchown(path, uid, gid);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_truncate(const char *path, off_t size) {
    int res;

    res = truncate(path, size);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_utimens(const char *path, const struct timespec ts[2]) {
    int res;
    struct timeval tv[2];

    tv[0].tv_sec = ts[0].tv_sec;
    tv[0].tv_usec = ts[0].tv_nsec / 1000;
    tv[1].tv_sec = ts[1].tv_sec;
    tv[1].tv_usec = ts[1].tv_nsec / 1000;

    res = utimes(path, tv);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi) {
    int res;

    res = open(path, fi->flags);
    if (res == -1)
        return -errno;

    close(res);
    return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi) {
    int fd;
    int res;

    (void) fi;
    fd = open(path, O_RDONLY);
    if (fd == -1)
        return -errno;

    res = pread(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    close(fd);
    return res;
}

static int xmp_write(const char *path, const char *buf, size_t size,
             off_t offset, struct fuse_file_info *fi) {
    int fd;
    int res;

    (void) fi;
    fd = open(path, O_WRONLY);
    if (fd == -1)
        return -errno;

    res = pwrite(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    close(fd);
    return res;
}

static int xmp_statfs(const char *path, struct statvfs *stbuf) {
    int res;

    res = statvfs(path, stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_create(const char* path, mode_t mode, struct fuse_file_info* fi) {

    (void) fi;

    int res;
    res = creat(path, mode);
    if(res == -1)
    return -errno;

    close(res);

    return 0;
}


static int xmp_release(const char *path, struct fuse_file_info *fi) {
    /* Just a stub. This method is optional and can safely be left unimplemented */
    (void) path;
    (void) fi;
    return 0;
}

static int xmp_fsync(const char *path, int isdatasync,
             struct fuse_file_info *fi) {
    /* Just a stub. This method is optional and can safely be left unimplemented */
    (void) path;
    (void) isdatasync;
    (void) fi;
    return 0;
}

#ifdef HAVE_SETXATTR
static int xmp_setxattr(const char *path, const char *name, const char *value,
            size_t size, int flags) {
    int res = lsetxattr(path, name, value, size, flags);
    if (res == -1)
        return -errno;
    return 0;
}

static int xmp_getxattr(const char *path, const char *name, char *value,
            size_t size) {
    int res = lgetxattr(path, name, value, size);
    if (res == -1)
        return -errno;
    return res;
}

static int xmp_listxattr(const char *path, char *list, size_t size) {
    int res = llistxattr(path, list, size);
    if (res == -1)
        return -errno;
    return res;
}

static int xmp_removexattr(const char *path, const char *name) {
    int res = lremovexattr(path, name);
    if (res == -1)
        return -errno;
    return 0;
}
#endif /* HAVE_SETXATTR */

static struct fuse_operations xmp_oper = {
    .getattr    = xmp_getattr,
    .access        = xmp_access,
    .readlink    = xmp_readlink,
    .readdir    = xmp_readdir,
    .mknod        = xmp_mknod,
    .mkdir        = xmp_mkdir,
    .symlink    = xmp_symlink,
    .unlink        = xmp_unlink,
    .rmdir        = xmp_rmdir,
    .rename        = xmp_rename,
    .link        = xmp_link,
    .chmod        = xmp_chmod,
    .chown        = xmp_chown,
    .truncate    = xmp_truncate,
    .utimens    = xmp_utimens,
    .open        = xmp_open,
    .read        = xmp_read,
    .write        = xmp_write,
    .statfs        = xmp_statfs,
    .create         = xmp_create,
    .release    = xmp_release,
    .fsync        = xmp_fsync,
#ifdef HAVE_SETXATTR
    .setxattr    = xmp_setxattr,
    .getxattr    = xmp_getxattr,
    .listxattr    = xmp_listxattr,
    .removexattr    = xmp_removexattr,
#endif
};

int main(int argc, char *argv[]) {
    char keyPhrase[KEY_PHRASE_MAX_LEN+1];
    char mirrorDirectoryPathString[PATH_MAX + 1];
    char *rv;
    umask(0);
    // Make sure we have enough arguments
    if ((argc < 4) || (argv[argc-3][0] == '-') || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-')) {
        printf("Usage:\n" \
            "\tpa5-encfs <key phrase> <mirror directory> <mount point>\n");
        return 0;
    }
    // Yank mirror directory
    if ((rv = realpath(argv[argc-2], mirrorDirectoryPathString))) {
        printf("Mirror directory: %s\n", mirrorDirectoryPathString);
        argv[argc-2] = argv[argc-1];
        argv[argc-1] = NULL;
        --argc;
    } else {
        perror("realpath");
        printf(ANSI_C_RED "You lied to me when you told me this was a directory.\n" ANSI_RESET);
        return(1);
    }
    // Yank key phrase
    strncpy (keyPhrase, argv[argc-3], KEY_PHRASE_MAX_LEN);
    printf("Key phrase: %s\n", keyPhrase);
    keyPhrase[KEY_PHRASE_MAX_LEN] = '\0';
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    --argc;
    // Mount point should be left behind
    printf("Mount point: %s\n", argv[argc-1]);
    // Call FUSE
    return fuse_main(argc, argv, &xmp_oper, NULL);
}
