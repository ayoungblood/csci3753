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
#include <limits.h> // MAX_PATH
#include <stdlib.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif
#include "aes-crypt.h"

/* Struct to store custom data across fuse calls */
struct fuse_data {
    char *key_phrase;
    char *mirror_directory;
};
/* Macro to get fuse data */
#define FUSE_DATA ((struct fuse_data*) fuse_get_context()->private_data)
/* XATTR name */
#define XATTR_ENCRYPTED "user.pa5-encfs.encrypted"

/* Takes a path and transforms it based on the mirror directory */
char *get_mirror_path(const char *path) {
    char *rv;
    int slen = strlen(path) + strlen(FUSE_DATA->mirror_directory) + 1;
    rv = malloc(sizeof(char) * slen);
    if (rv == NULL) return NULL;
    strncpy(rv, FUSE_DATA->mirror_directory, slen);
    strncat(rv, path, slen);
    //printf("get_mirror_path: %s\n",rv);
    return rv;
}

static int xmp_getattr(const char *path, struct stat *stbuf) {
    int res;
    char* mpath = get_mirror_path(path);
    //printf("xmp_getattr: %s\n",mpath);
    res = lstat(mpath, stbuf);
    if (res == -1)
        return -errno;

    free(mpath);
    return 0;
}

static int xmp_access(const char *path, int mask) {
    int res;
    char* mpath = get_mirror_path(path);
    //printf("xmp_access: %s\n",mpath);
    res = access(mpath, mask);
    if (res == -1)
        return -errno;

    free(mpath);
    return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size) {
    int res;
    char* mpath = get_mirror_path(path);
    //printf("xmp_readlink: %s\n",mpath);
    res = readlink(mpath, buf, size - 1);
    if (res == -1)
        return -errno;

    buf[res] = '\0';
    free(mpath);
    return 0;
}


static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
               off_t offset, struct fuse_file_info *fi) {
    DIR *dp;
    struct dirent *de;
    (void) offset;
    (void) fi;

    char* mpath = get_mirror_path(path);
    //printf("xmp_readdir: %s\n",mpath);
    dp = opendir(mpath);
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
    free(mpath);
    return 0;
}

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev) {
    int res;
    char* mpath = get_mirror_path(path);
    //printf("xmp_mknod: %s\n",mpath);
    /* On Linux this could just be 'mknod(path, mode, rdev)' but this
       is more portable */
    if (S_ISREG(mode)) {
        res = open(mpath, O_CREAT | O_EXCL | O_WRONLY, mode);
        if (res >= 0)
            res = close(res);
    } else if (S_ISFIFO(mode))
        res = mkfifo(mpath, mode);
    else
        res = mknod(mpath, mode, rdev);
    if (res == -1)
        return -errno;
    free(mpath);
    return 0;
}

static int xmp_mkdir(const char *path, mode_t mode) {
    int res;
    char* mpath = get_mirror_path(path);
    //printf("xmp_mkdir: %s\n",mpath);
    res = mkdir(mpath, mode);
    if (res == -1)
        return -errno;

    free(mpath);
    return 0;
}

static int xmp_unlink(const char *path) {
    int res;
    char* mpath = get_mirror_path(path);
    //printf("xmp_unlink: %s\n",mpath);
    res = unlink(mpath);
    if (res == -1)
        return -errno;

    free(mpath);
    return 0;
}

static int xmp_rmdir(const char *path) {
    int res;
    char* mpath = get_mirror_path(path);
    //printf("xmp_rmdir: %s\n",mpath);
    res = rmdir(mpath);
    if (res == -1)
        return -errno;

    free(mpath);
    return 0;
}

static int xmp_symlink(const char *from, const char *to) {
    int res;
    char* mfrom = get_mirror_path(from);
    //printf("xmp_symlink (from): %s\n",mfrom);
    char* mto = get_mirror_path(to);
    //printf("xmp_symlink (to): %s\n",mto);
    res = symlink(mfrom, mto);
    if (res == -1)
        return -errno;

    free(mfrom);
    free(mto);
    return 0;
}

static int xmp_rename(const char *from, const char *to) {
    int res;
    char* mfrom = get_mirror_path(from);
    //printf("xmp_symlink (from): %s\n",mfrom);
    char* mto = get_mirror_path(to);
    //printf("xmp_symlink (to): %s\n",mto);
    res = rename(from, to);
    if (res == -1)
        return -errno;

    free(mfrom);
    free(mto);
    return 0;
}

static int xmp_link(const char *from, const char *to) {
    int res;
    char* mfrom = get_mirror_path(from);
    //printf("xmp_symlink (from): %s\n",mfrom);
    char* mto = get_mirror_path(to);
    //printf("xmp_symlink (to): %s\n",mto);
    res = link(from, to);
    if (res == -1)
        return -errno;

    free(mfrom);
    free(mto);
    return 0;
}

static int xmp_chmod(const char *path, mode_t mode) {
    int res;
    char* mpath = get_mirror_path(path);
    //printf("xmp_chmod: %s\n",mpath);
    res = chmod(mpath, mode);
    if (res == -1)
        return -errno;

    free(mpath);
    return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid) {
    int res;
    char* mpath = get_mirror_path(path);
    //printf("xmp_chown: %s\n",mpath);
    res = lchown(mpath, uid, gid);
    if (res == -1)
        return -errno;

    free(mpath);
    return 0;
}

static int xmp_truncate(const char *path, off_t size) {
    int res;
    char* mpath = get_mirror_path(path);
    //printf("xmp_truncate: %s\n",mpath);
    res = truncate(mpath, size);
    if (res == -1)
        return -errno;

    free(mpath);
    return 0;
}

static int xmp_utimens(const char *path, const struct timespec ts[2]) {
    int res;
    struct timeval tv[2];

    tv[0].tv_sec = ts[0].tv_sec;
    tv[0].tv_usec = ts[0].tv_nsec / 1000;
    tv[1].tv_sec = ts[1].tv_sec;
    tv[1].tv_usec = ts[1].tv_nsec / 1000;
    char* mpath = get_mirror_path(path);
    //printf("xmp_utimens: %s\n",mpath);
    res = utimes(mpath, tv);
    if (res == -1)
        return -errno;

    free(mpath);
    return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi) {
    int res;
    char* mpath = get_mirror_path(path);
    //printf("xmp_open: %s\n",mpath);
    res = open(mpath, fi->flags);
    if (res == -1)
        return -errno;

    close(res);
    free(mpath);
    return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi) {
    (void) fi;
    int res;
    // Get the actual path
    char* mpath = get_mirror_path(path);
    fprintf(stderr, "xmp_read: %s\n",mpath);
    // Open the file we are reading from
    FILE *fp = fopen(mpath,"r");
    if (NULL == fp) {
        fprintf(stderr, "xmp_read: Failed to open %s\n",mpath);
        return -errno;
    }
    // Get a temp file for decryption
    FILE *tp = tmpfile();
    if (NULL == tp) {
        fprintf(stderr, "xmp_read: Failed to open temp file\n");
        return -errno;
    }
    // Check the xattr
    // (from https://www.cocoanetics.com/2012/03/reading-and-writing-extended-file-attributes/)
    char is_encrypted = 0; // bool is overrated
    // getxattr returns size in bytes if the xattr exists and -1 if not
    int xattr_size = getxattr(mpath, XATTR_ENCRYPTED, NULL, 0);
    printf("xmp_read: xattr %s has size %d\n",XATTR_ENCRYPTED,xattr_size);
    if (xattr_size > 0) { // if the xattr exists, check it
        char *xattr_buf = malloc(xattr_size);
        getxattr(mpath, XATTR_ENCRYPTED, xattr_buf, xattr_size);
        printf("xmp_read: xattr %s is %s\n", XATTR_ENCRYPTED, xattr_buf);
        if (!strncmp(xattr_buf,"true",4)) {
            is_encrypted = 1;
        }
        free(xattr_buf);
    }
    printf("xmp_read: from xattr %s, file is %s\n",XATTR_ENCRYPTED,is_encrypted?"encrypted":"not encrypted");
    // Decrypt if necessary
    if (is_encrypted) { // File is encrypted
        fprintf(stderr, "xmp_read: File is encrypted, decrypting to temp\n");
        // Decrypt the file to a tempfile
        if (FAILURE == do_crypt(fp, tp, 0, FUSE_DATA->key_phrase)) {
            fprintf(stderr, "xmp_read: Failed to decrypt %s\n",mpath);
            return -errno;
        }
    } else { // File is not encrypted
        fprintf(stderr, "xmp_read: File is not encrypted, passthrough to temp\n");
        // Passthrough the file to a tempfile
        if (FAILURE == do_crypt(fp, tp, -1, FUSE_DATA->key_phrase)) {
            fprintf(stderr, "xmp_read: Failed to passthrough %s\n",mpath);
            return -errno;
        }
    }
    // Read the decrypted file
    fflush(tp);
    fseek(tp, offset, SEEK_SET);
    res = fread(buf, 1, size, tp);
    if (res == -1) {
        res = -errno;
    }
    // Clean up
    fclose(fp);
    fclose(tp);
    free(mpath);
    return res;
}

static int xmp_write(const char *path, const char *buf, size_t size,
             off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    int res;
    // Get the actual path
    char* mpath = get_mirror_path(path);
    fprintf(stderr, "xmp_write: %s\n",mpath);
    // Open the file we are writing to
    FILE *fp = fopen(mpath, "r+");
    if (NULL == fp) {
        fprintf(stderr, "xmp_write: Failed to open %s\n",mpath);
        return -errno;
    }
    // Get a temp file for decryption
    FILE *tp = tmpfile();
    int tp_descriptor = fileno(tp); // need the descriptor for writing
    if (NULL == tp) {
        fprintf(stderr, "xmp_write: Failed to open temp file\n");
        return -errno;
    }
    // Check the xattr
    // (from https://www.cocoanetics.com/2012/03/reading-and-writing-extended-file-attributes/)
    char is_encrypted = 0; // bool is overrated
    // getxattr returns size in bytes if the xattr exists and -1 if not
    int xattr_size = getxattr(mpath, XATTR_ENCRYPTED, NULL, 0);
    printf("xmp_read: xattr %s has size %d\n",XATTR_ENCRYPTED,xattr_size);
    if (xattr_size > 0) { // if the xattr exists, check it
        char *xattr_buf = malloc(xattr_size);
        getxattr(mpath, XATTR_ENCRYPTED, xattr_buf, xattr_size);
        printf("xmp_read: xattr %s is %s\n", XATTR_ENCRYPTED, xattr_buf);
        if (!strncmp(xattr_buf,"true",4)) {
            is_encrypted = 1;
        }
        free(xattr_buf);
    }
    printf("xmp_read: from xattr %s, file is %s\n",XATTR_ENCRYPTED,is_encrypted?"encrypted":"not encrypted");
    // Decrypt if necessary (see https://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/))
    if (size > 0) {
        if (is_encrypted) { // File is encrypted
            fprintf(stderr, "xmp_write: File is encrypted, decrypting to temp\n");
            // Decrypt the file to a tempfile
            if (FAILURE == do_crypt(fp, tp, 0, FUSE_DATA->key_phrase)) {
                fprintf(stderr, "xmp_write: Failed to decrypt %s\n",mpath);
                return -errno;
            }
            rewind(fp);
            rewind(tp);
        } else { // File is not encrypted
            fprintf(stderr, "xmp_write: File is not encrypted, do_crypt passthrough\n");
            // Passthrough the file to a tempfile
            if (FAILURE == do_crypt(fp, tp, -1, FUSE_DATA->key_phrase)) {
                fprintf(stderr, "xmp_write: Failed to passthrough %s\n",mpath);
                return -errno;
            }
            rewind(fp);
            rewind(tp);
        }
    }
    // Do the write
    res = pwrite(tp_descriptor, buf, size, offset);
    if (res == -1)
        res = -errno;
    // Encrypt if necessary
    if (is_encrypted) { // File is encrypted
        fprintf(stderr, "xmp_write: File was encrypted, re-encrypting from temp\n");
        // Encrypt the temp file to the actual file
        if (FAILURE == do_crypt(tp, fp, 1, FUSE_DATA->key_phrase)) {
            fprintf(stderr, "xmp_write: Failed to encrypt %s\n",mpath);
            return -errno;
        }
    } else { // File is not encrypted
        fprintf(stderr, "xmp_write: File was not encrypted, do_crypt passthrough\n");
        // Passthrough the tempfile to file
        if (FAILURE == do_crypt(tp, fp, -1, FUSE_DATA->key_phrase)) {
            fprintf(stderr, "xmp_write: Failed to passthrough %s\n",mpath);
            return -errno;
        }
    }
    // Clean up
    fclose(fp);
    fclose(tp);
    free(mpath);
    return res;
}

static int xmp_statfs(const char *path, struct statvfs *stbuf) {
    int res;
    char* mpath = get_mirror_path(path);
    //printf("xmp_statfs: %s\n",mpath);
    res = statvfs(mpath, stbuf);
    if (res == -1)
        return -errno;

    free(mpath);
    return 0;
}

static int xmp_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
    (void) fi;
    (void) mode;
    // Get the actual path
    char* mpath = get_mirror_path(path);
    fprintf(stderr, "xmp_create: %s\n",mpath);
    // Create encrypted file
    FILE *fp = fopen(mpath,"wb+");
    if (FAILURE == do_crypt(fp, fp, 1, FUSE_DATA->key_phrase)) {
        fprintf(stderr, "xmp_create: Failed to encrypt %s\n",mpath);
        return -errno;
    }
    fprintf(stderr, "xmp_create: Encrypted %s\n",mpath);
    // Add xattr
    if (-1 == setxattr(mpath, XATTR_ENCRYPTED, "true ", 6, 0)){
        fprintf(stderr, "xmp_create: Failed to set xattr %s on %s\n", XATTR_ENCRYPTED, mpath);
        return -errno;
    }
    fprintf(stderr, "xmp_create: Set xattr %s on %s\n", XATTR_ENCRYPTED, mpath);
    // Clean up
    fclose(fp);
    free(mpath);
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
    char* mpath = get_mirror_path(path);
    fprintf(stderr, "xmp_setxattr: %s\n",mpath);
    int res = lsetxattr(mpath, name, value, size, flags);
    if (res == -1)
        return -errno;
    free(mpath);
    return 0;
}

static int xmp_getxattr(const char *path, const char *name, char *value,
            size_t size) {
    char* mpath = get_mirror_path(path);
    fprintf(stderr, "xmp_getxattr: %s\n",mpath);
    int res = lgetxattr(mpath, name, value, size);
    if (res == -1)
        return -errno;
    free(mpath);
    return res;
}

static int xmp_listxattr(const char *path, char *list, size_t size) {
    char* mpath = get_mirror_path(path);
    fprintf(stderr, "xmp_listxattr: %s\n",mpath);
    int res = llistxattr(mpath, list, size);
    if (res == -1)
        return -errno;
    free(mpath);
    return res;
}

static int xmp_removexattr(const char *path, const char *name) {
    char* mpath = get_mirror_path(path);
    fprintf(stderr, "xmp_removexattr: %s\n",mpath);
    int res = lremovexattr(mpath, name);
    if (res == -1)
        return -errno;
    free(mpath);
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
    umask(0);
    // Make sure we have enough arguments
    if ((argc < 4) || (argv[argc-3][0] == '-') || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-')) {
        printf("Usage:\n" \
            "\tpa5-encfs <key phrase> <mirror directory> <mount point>\n\n");
        return 0;
    }
    // Initialize data struct
    struct fuse_data *fuse_data;
    fuse_data = malloc(sizeof(struct fuse_data));
    if (fuse_data == NULL) {
        fprintf(stderr, "Failed to allocate memory. Exiting.\n");
        return 1;
    }
    // Yank mirror directory (from J. J. Pfeiffer, https://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/)
    if ((fuse_data->mirror_directory = realpath(argv[argc-2], NULL))) {
        printf("Mirror directory: %s\n", fuse_data->mirror_directory);
        argv[argc-2] = argv[argc-1];
        argv[argc-1] = NULL;
        --argc;
    } else {
        perror("realpath");
        fprintf(stderr, "You lied to me when you told me this was a directory.\n");
        return 1;
    }
    // Yank key phrase
    fuse_data->key_phrase = argv[argc-3];
    printf("Key phrase: %s\n", fuse_data->key_phrase);
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    --argc;
    // Mount point should be left behind as last arg
    printf("Mount point: %s\n", argv[argc-1]);
    // Call FUSE
    return fuse_main(argc, argv, &xmp_oper, fuse_data);
}
